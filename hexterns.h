/* HEXTERNS.H   (c) Copyright Roger Bowler, 1999-2010                */
/*                    Hercules function prototypes...                */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

//      This header auto-#included by 'hercules.h'...
//
//      The <config.h> header and other required headers are
//      presumed to have already been #included ahead of it...

// $Id$

#ifndef _HEXTERNS_H
#define _HEXTERNS_H

#include "hercules.h"

// Define all DLL Imports depending on current file

#ifndef _HSYS_C_
#define HSYS_DLL_IMPORT DLL_IMPORT
#else   /* _HSYS_C_ */
#define HSYS_DLL_IMPORT DLL_EXPORT
#endif  /* _HSYS_C_ */

#ifndef _CCKDDASD_C_
#ifndef _HDASD_DLL_
#define CCKD_DLL_IMPORT DLL_IMPORT
#else   /* _HDASD_DLL_ */
#define CCKD_DLL_IMPORT extern
#endif  /* _HDASD_DLL_ */
#else
#define CCKD_DLL_IMPORT DLL_EXPORT
#endif

#ifndef _HDL_C_
#ifndef _HUTIL_DLL_
#define HHDL_DLL_IMPORT DLL_IMPORT
#else   /* _HDASD_DLL_ */
#define HHDL_DLL_IMPORT extern
#endif  /* _HDASD_DLL_ */
#else
#define HHDL_DLL_IMPORT DLL_EXPORT
#endif

#ifndef _HSCCMD_C_
#ifndef _HENGINE_DLL_
#define HCMD_DLL_IMPORT DLL_IMPORT
#else   /* _HENGINE_DLL_ */
#define HCMD_DLL_IMPORT extern
#endif  /* _HENGINE_DLL_ */
#else
#define HCMD_DLL_IMPORT DLL_EXPORT
#endif

#ifndef _CMDTAB_C_
#ifndef _HENGINE_DLL_
#define CMDT_DLL_IMPORT DLL_IMPORT
#else   /* _HENGINE_DLL_ */
#define CMDT_DLL_IMPORT extern
#endif  /* _HENGINE_DLL_ */
#else
#define CMDT_DLL_IMPORT DLL_EXPORT
#endif

#ifndef _HAO_C_
#ifndef _HENGINE_DLL_
#define HAO_DLL_IMPORT DLL_IMPORT
#else   /* _HENGINE_DLL_ */
#define HAO_DLL_IMPORT extern
#endif  /* _HENGINE_DLL_ */
#else
#define HAO_DLL_IMPORT DLL_EXPORT
#endif

#ifndef _PANEL_C_
#ifndef _HENGINE_DLL_
#define HPAN_DLL_IMPORT DLL_IMPORT
#else   /* _HENGINE_DLL_ */
#define HPAN_DLL_IMPORT extern
#endif  /* _HENGINE_DLL_ */
#else
#define HPAN_DLL_IMPORT DLL_EXPORT
#endif

#ifndef _IMPL_C_
#ifndef _HENGINE_DLL_
#define IMPL_DLL_IMPORT DLL_IMPORT
#else   /* _HENGINE_DLL_ */
#define IMPL_DLL_IMPORT extern
#endif  /* _HENGINE_DLL_ */
#else
#define IMPL_DLL_IMPORT DLL_EXPORT
#endif

#ifndef _CCKDUTIL_C_
#ifndef _HDASD_DLL_
#define CCDU_DLL_IMPORT DLL_IMPORT
#else   /* _HDASD_DLL_ */
#define CCDU_DLL_IMPORT extern
#endif  /* _HDASD_DLL_ */
#else
#define CCDU_DLL_IMPORT DLL_EXPORT
#endif

#ifndef _CONFIG_C_
#ifndef _HENGINE_DLL_
#define CONF_DLL_IMPORT DLL_IMPORT
#else   /* _HDASD_DLL_ */
#define CONF_DLL_IMPORT extern
#endif  /* _HDASD_DLL_ */
#else
#define CONF_DLL_IMPORT DLL_EXPORT
#endif

#ifndef _SCRIPT_C_
#ifndef _HENGINE_DLL_
#define SCRI_DLL_IMPORT DLL_IMPORT
#else   /* _SCRIPT_DLL_ */
#define SCRI_DLL_IMPORT extern
#endif  /* _SCRIPT_DLL_ */
#else
#define SCRI_DLL_IMPORT DLL_EXPORT
#endif

#ifndef _BLDCFG_C_
#ifndef _HENGINE_DLL_
#define BLDC_DLL_IMPORT DLL_IMPORT
#else   /* _HDASD_DLL_ */
#define BLDC_DLL_IMPORT extern
#endif  /* _HDASD_DLL_ */
#else
#define BLDC_DLL_IMPORT DLL_EXPORT
#endif

#ifndef _SERVICE_C_
#ifndef _HENGINE_DLL_
#define SERV_DLL_IMPORT DLL_IMPORT
#else   /* _HENGINE_DLL_ */
#define SERV_DLL_IMPORT extern
#endif  /* _HENGINE_DLL_ */
#else
#define SERV_DLL_IMPORT DLL_EXPORT
#endif

#ifndef _LOADPARM_C_
#ifndef _HENGINE_DLL_
#define LOADPARM_DLL_IMPORT DLL_IMPORT
#else   /* _HENGINE_DLL_ */
#define LOADPARM_DLL_IMPORT extern
#endif  /* _HENGINE_DLL_ */
#else
#define LOADPARM_DLL_IMPORT DLL_EXPORT
#endif

#if defined( _MSC_VER ) && (_MSC_VER >= 1300) && (_MSC_VER < 1400)
//  '_ftol'   is defined in MSVCRT.DLL
//  '_ftol2'  we define ourselves in "w32ftol2.c"
extern long _ftol ( double dblSource );
extern long _ftol2( double dblSource );
#endif

#if !defined(HAVE_STRSIGNAL)
  const char* strsignal(int signo);    // (ours is in 'strsignal.c')
#endif

#if defined(HAVE_SETRESUID)
/* (the following missing from SUSE 7.1) */
int getresuid(uid_t *ruid, uid_t *euid, uid_t *suid);
int getresgid(gid_t *rgid, gid_t *egid, gid_t *sgid);
int setresuid(uid_t ruid, uid_t euid, uid_t suid);
int setresgid(gid_t rgid, gid_t egid, gid_t sgid);
#endif

/* Function used to compare filenames */
#if defined(MIXEDCASE_FILENAMES_ARE_UNIQUE)
  #define strfilenamecmp  strcmp
  #define strnfilenamecmp  strncmp
#else
  #define strfilenamecmp  strcasecmp
  #define strnfilenamecmp  strncasecmp
#endif

/* Global data areas in module config.c                              */
HSYS_DLL_IMPORT SYSBLK   sysblk;         /* System control block      */
CCKD_DLL_IMPORT CCKDBLK  cckdblk;                /* CCKD global block         */
#ifdef EXTERNALGUI
HSYS_DLL_IMPORT int extgui;             // __attribute__ ((deprecated));
/* The external gui interface is now external and now uses the
   HDC(debug_cpu_state, regs) interface */
#endif /*EXTERNALGUI*/

/* Functions in module bldcfg.c */
#if defined( OPTION_TAPE_AUTOMOUNT )
BLDC_DLL_IMPORT int add_tamdir( char *tamdir, TAMDIR **ppTAMDIR );
#endif /* OPTION_TAPE_AUTOMOUNT */
int build_config (char *fname);

/* Functions in module script.c */
SCRI_DLL_IMPORT int process_config (char *fname);
SCRI_DLL_IMPORT int parse_args (char* p, int maxargc, char** pargv, int* pargc);
 
/* Functions in module config.c */
void release_config ();
CONF_DLL_IMPORT DEVBLK *find_device_by_devnum (U16 lcss, U16 devnum);
DEVBLK *find_device_by_subchan (U32 ioid);
REGS *devregs(DEVBLK *dev);
DEVBLK *get_devblk (U16 lcss, U16 devnum);
void ret_devblk (DEVBLK *dev);
int  attach_device (U16 lcss, U16 devnum, const char *devtype, int addargc,
        char *addargv[]);
int  detach_subchan (U16 lcss, U16 subchan, U16 devnum);
int  detach_device (U16 lcss, U16 devnum);
int  define_device (U16 lcss, U16 olddev, U16 newdev);
CONF_DLL_IMPORT int  group_device(DEVBLK *dev, int members);
int  configure_cpu (int cpu);
int  deconfigure_cpu (int cpu);
int  configure_numcpu (int numcpu);
int  configure_storage(RADR);
int  configure_xstorage(U32);
int  configure_capping(U32 value);
int  configure_herc_priority(int prio);
int  configure_shrdport(U16 shrdport);
#define MAX_ARGS  12                    /* Max argv[] array size     */
int parse_and_attach_devices(const char *devnums,const char *devtype,int ac,char **av);
CONF_DLL_IMPORT int parse_single_devnum(const char *spec, U16 *lcss, U16 *devnum);
int parse_single_devnum_silent(const char *spec, U16 *lcss, U16 *devnum);
int readlogo(char *fn);
void clearlogo(void);
CONF_DLL_IMPORT int parse_conkpalv(char* s, int* idle, int* intv, int* cnt );

/* Functions in module archlvl.c */
int set_archlvl(char *archname);
void init_als(REGS *regs);
BYTE als_update_pending(void);

/* Global data areas and functions in module cpu.c                   */
extern const char* arch_name[];
extern const char* get_arch_mode_string(REGS* regs);

/* Functions in module panel.c */
void expire_kept_msgs(int unconditional);
void set_console_title(char * status);
#ifdef OPTION_MIPS_COUNTING
HPAN_DLL_IMPORT U32    maxrates_rpt_intvl;  // (reporting interval)
HPAN_DLL_IMPORT U32    curr_high_mips_rate; // (high water mark for current interval)
HPAN_DLL_IMPORT U32    curr_high_sios_rate; // (high water mark for current interval)
HPAN_DLL_IMPORT U32    prev_high_mips_rate; // (saved high water mark for previous interval)
HPAN_DLL_IMPORT U32    prev_high_sios_rate; // (saved high water mark for previous interval)
HPAN_DLL_IMPORT time_t curr_int_start_time; // (start time of current interval)
HPAN_DLL_IMPORT time_t prev_int_start_time; // (start time of previous interval)
HPAN_DLL_IMPORT void update_maxrates_hwm(); // (update high-water-mark values)
#endif // OPTION_MIPS_COUNTING

/* Functions in module hao.c (Hercules Automatic Operator) */
#if defined(OPTION_HAO)
HAO_DLL_IMPORT int hao_initialize(void);        /* initialize hao */
HAO_DLL_IMPORT void hao_command(char *command); /* process hao command */
HAO_DLL_IMPORT void hao_message(char *message); /* process message */
#endif /* defined(OPTION_HAO) */

/* Functions in module hsccmd.c (so PTT debugging patches can access them) */
HCMD_DLL_IMPORT int aia_cmd     (int argc, char *argv[], char *cmdline);
HCMD_DLL_IMPORT int stopall_cmd (int argc, char *argv[], char *cmdline);

/* Functions in module cmdtab.c */
CMDT_DLL_IMPORT int ProcessConfigCommand (int argc, char **argv, char *cmdline);
CMDT_DLL_IMPORT int ProcessConfigCmdLine (char *cmdline);
CMDT_DLL_IMPORT int ProcessPanelCommand (char *cmdline);

/* Functions in module losc.c */
#if defined(OPTION_LPP_RESTRICT)
void losc_set (int license_status);
void losc_check(char *ostype);
#endif /*defined(OPTION_LPP_RESTRICT)*/

#if defined(OPTION_DYNAMIC_LOAD)

HHDL_DLL_IMPORT char *(*hdl_device_type_equates) (const char *);
CMDT_DLL_IMPORT void *(panel_command_r)          (void *cmdline);
HPAN_DLL_IMPORT void  (panel_display_r)          (void);

HSYS_DLL_IMPORT int   (*config_command) (int argc, char *argv[], char *cmdline);
HSYS_DLL_IMPORT int   (*system_command) (int argc, char *argv[], char *cmdline);
HSYS_DLL_IMPORT void  (*daemon_task)    (void);
HSYS_DLL_IMPORT void  (*panel_display)  (void);
HSYS_DLL_IMPORT void *(*panel_command)  (void *);

HSYS_DLL_IMPORT void *(*debug_device_state)         (DEVBLK *);
HSYS_DLL_IMPORT void *(*debug_cpu_state)            (REGS *);
HSYS_DLL_IMPORT void *(*debug_cd_cmd)               (char *);
HSYS_DLL_IMPORT void *(*debug_watchdog_signal)      (REGS *);
HSYS_DLL_IMPORT void *(*debug_program_interrupt)    (REGS *, int);
HSYS_DLL_IMPORT void *(*debug_diagnose)             (U32, int,  int, REGS *);
HSYS_DLL_IMPORT void *(*debug_iucv)                 (int, VADR, REGS *);
HSYS_DLL_IMPORT void *(*debug_sclp_unknown_command) (U32,    void *, REGS *);
HSYS_DLL_IMPORT void *(*debug_sclp_unknown_event)   (void *, void *, REGS *);
HSYS_DLL_IMPORT void *(*debug_sclp_unknown_event_mask) (void *, void *, REGS *);
HSYS_DLL_IMPORT void *(*debug_chsc_unknown_request) (void *, void *, REGS *);
HSYS_DLL_IMPORT void *(*debug_sclp_event_data)      (void *, void *, REGS *);

#else
void *panel_command (void *cmdline);
void panel_display (void);
#define debug_cpu_state                 NULL
#define debug_cd_cmd                    NULL
#define debug_device_state              NULL
#define debug_program_interrupt         NULL
#define debug_diagnose                  NULL
#define debug_iucv                      NULL
#define debug_sclp_unknown_command      NULL
#define debug_sclp_unknown_event        NULL
#define debug_sclp_event_data           NULL
#define debug_chsc_unknown_request      NULL
#define debug_watchdog_signal           NULL
#endif

/* Functions in module httpserv.c */
int http_command(int argc, char *argv[]);
int http_startup(int isconfigcalling);

/* Functions in module loadparm.c */
void set_loadparm(char *name);
void get_loadparm(BYTE *dest);
char *str_loadparm();
void set_lparname(char *name);
void get_lparname(BYTE *dest);
LOADPARM_DLL_IMPORT char *str_lparname();
int set_manufacturer(char *name);
LOADPARM_DLL_IMPORT char *str_manufacturer();
int set_plant(char *name);
LOADPARM_DLL_IMPORT char *str_plant();
int set_model(char *m1, char* m2, char* m3, char* m4);
LOADPARM_DLL_IMPORT char **str_model();
char *str_modelhard();
char *str_modelcapa();
char *str_modelperm();
char *str_modeltemp();
void get_manufacturer(BYTE *name);
void get_plant(BYTE *name);
void get_model(BYTE *name);
void get_modelcapa(BYTE *name);
void get_modelperm(BYTE *name);
void get_modeltemp(BYTE *name);
void get_sysname(BYTE *name);
void get_systype(BYTE *name);
void get_sysplex(BYTE *name);
void set_sysname(BYTE *name);
void set_systype(BYTE *name);
void set_sysplex(BYTE *name);
LOADPARM_DLL_IMPORT char *str_sysname();
LOADPARM_DLL_IMPORT char *str_sysplex();
LOADPARM_DLL_IMPORT char *str_systype();
void get_mpfactors(BYTE *dest);

/* Functions in module impl.c */
IMPL_DLL_IMPORT void system_cleanup(void);

typedef void (*LOGCALLBACK)(const char *,size_t);
typedef void *(*COMMANDHANDLER)(void *);

IMPL_DLL_IMPORT int impl(int,char **);
IMPL_DLL_IMPORT void regiserLogCallback(LOGCALLBACK);
IMPL_DLL_IMPORT COMMANDHANDLER getCommandHandler(void);

/* Functions in module timer.c */
void update_TOD_clock (void);
void *timer_update_thread (void *argp);
void *capping_manager_thread(void *argp);

/* Functions in module service.c */
void scp_command (char *command, int priomsg, int echo);
int can_signal_quiesce ();
int can_send_command ();
int signal_quiesce (U16 count, BYTE unit);
void sclp_attention(U16 type);
void sclp_reset();
SERV_DLL_IMPORT void sclp_sysg_attention();
int servc_hsuspend(void *file);
int servc_hresume(void *file);

/* Functions in module ckddasd.c */
void ckd_build_sense ( DEVBLK *, BYTE, BYTE, BYTE, BYTE, BYTE);
int ckddasd_init_handler ( DEVBLK *dev, int argc, char *argv[]);
void ckddasd_execute_ccw ( DEVBLK *dev, BYTE code, BYTE flags,
        BYTE chained, U16 count, BYTE prevcode, int ccwseq,
        BYTE *iobuf, BYTE *more, BYTE *unitstat, U16 *residual );
int ckddasd_close_device ( DEVBLK *dev );
void ckddasd_query_device (DEVBLK *dev, char **class,
                int buflen, char *buffer);
int ckddasd_hsuspend ( DEVBLK *dev, void *file );
int ckddasd_hresume  ( DEVBLK *dev, void *file );

/* Functions in module fbadasd.c */
FBA_DLL_IMPORT void fbadasd_syncblk_io (DEVBLK *dev, BYTE type, int blknum,
        int blksize, BYTE *iobuf, BYTE *unitstat, U16 *residual);
FBA_DLL_IMPORT void fbadasd_read_block 
      ( DEVBLK *dev, int blknum, int blksize, int blkfactor, 
        BYTE *iobuf, BYTE *unitstat, U16 *residual );
FBA_DLL_IMPORT void fbadasd_write_block 
      ( DEVBLK *dev, int blknum, int blksize, int blkfactor, 
        BYTE *iobuf, BYTE *unitstat, U16 *residual );
int fbadasd_init_handler ( DEVBLK *dev, int argc, char *argv[]);
void fbadasd_execute_ccw ( DEVBLK *dev, BYTE code, BYTE flags,
        BYTE chained, U16 count, BYTE prevcode, int ccwseq,
        BYTE *iobuf, BYTE *more, BYTE *unitstat, U16 *residual );
int fbadasd_close_device ( DEVBLK *dev );
void fbadasd_query_device (DEVBLK *dev, char **class,
                int buflen, char *buffer);
int fbadasd_hsuspend ( DEVBLK *dev, void *file );
int fbadasd_hresume  ( DEVBLK *dev, void *file );

/* Functions in module cckddasd.c */
DEVIF   cckddasd_init_handler;
int     cckddasd_close_device (DEVBLK *);
int     cckd_read_track (DEVBLK *, int, BYTE *);
int     cckd_update_track (DEVBLK *, int, int, BYTE *, int, BYTE *);
int     cfba_read_block (DEVBLK *, int, BYTE *);
int     cfba_write_block (DEVBLK *, int, int, BYTE *, int, BYTE *);
CCKD_DLL_IMPORT void   *cckd_sf_add (void *);
CCKD_DLL_IMPORT void   *cckd_sf_remove (void *);
CCKD_DLL_IMPORT void   *cckd_sf_stats (void *);
CCKD_DLL_IMPORT void   *cckd_sf_comp (void *);
CCKD_DLL_IMPORT void   *cckd_sf_chk (void *);
CCKD_DLL_IMPORT int     cckd_command(char *, int);
CCKD_DLL_IMPORT void    cckd_print_itrace ();

/* Functions in module cckdutil.c */
CCDU_DLL_IMPORT int     cckd_swapend (DEVBLK *);
CCDU_DLL_IMPORT void    cckd_swapend_chdr (CCKD_DEVHDR *);
CCDU_DLL_IMPORT void    cckd_swapend_l1 (CCKD_L1ENT *, int);
CCDU_DLL_IMPORT void    cckd_swapend_l2 (CCKD_L2ENT *);
CCDU_DLL_IMPORT void    cckd_swapend_free (CCKD_FREEBLK *);
CCDU_DLL_IMPORT void    cckd_swapend4 (char *);
CCDU_DLL_IMPORT void    cckd_swapend2 (char *);
CCDU_DLL_IMPORT int     cckd_endian ();
CCDU_DLL_IMPORT int     cckd_comp (DEVBLK *);
CCDU_DLL_IMPORT int     cckd_chkdsk (DEVBLK *, int);
CCDU_DLL_IMPORT void    cckdumsg (DEVBLK *, int, char *, ...);

/* Functions in module hscmisc.c */
int herc_system (char* command);
void do_shutdown();
int display_regs (REGS *regs, char *buf, int buflen, char *hdr);
int display_fregs (REGS *regs, char *buf, int buflen,char *hdr);
int display_cregs (REGS *regs, char *buf, int buflen, char *hdr);
int display_aregs (REGS *regs, char *buf, int buflen, char *hdr);
int display_subchannel (DEVBLK *dev, char *buf, int buflen, char *hdr);
void get_connected_client (DEVBLK* dev, char** pclientip, char** pclientname);
void alter_display_real (char *opnd, REGS *regs);
void alter_display_virt (char *opnd, REGS *regs);
void disasm_stor(REGS *regs, char *opnd);
int drop_privileges(int capa);

/* Functions in module rexx.c */
int exec_cmd(int argc, char *argv[],char *cmdline);

/* Functions in module sr.c */
int suspend_cmd(int argc, char *argv[],char *cmdline);
int resume_cmd(int argc, char *argv[],char *cmdline);

/* Functions in ecpsvm.c that are not *direct* instructions */
/* but support functions either used by other instruction   */
/* functions or from somewhere else                         */
#ifdef FEATURE_ECPSVM
int  ecpsvm_dosvc(REGS *regs, int svccode);
int  ecpsvm_dossm(REGS *regs,int b,VADR ea);
int  ecpsvm_dolpsw(REGS *regs,int b,VADR ea);
int  ecpsvm_dostnsm(REGS *regs,int b,VADR ea,int imm);
int  ecpsvm_dostosm(REGS *regs,int b,VADR ea,int imm);
int  ecpsvm_dosio(REGS *regs,int b,VADR ea);
int  ecpsvm_dodiag(REGS *regs,int r1,int r3,int b2,VADR effective_addr2);
int  ecpsvm_dolctl(REGS *regs,int r1,int r3,int b2,VADR effective_addr2);
int  ecpsvm_dostctl(REGS *regs,int r1,int r3,int b2,VADR effective_addr2);
int  ecpsvm_doiucv(REGS *regs,int b2,VADR effective_addr2);
int  ecpsvm_virttmr_ext(REGS *regs);
#endif

/* Functions in module w32ctca.c */
#if defined(OPTION_W32_CTCI)
HSYS_DLL_IMPORT int  (*debug_tt32_stats)   (int);
HSYS_DLL_IMPORT void (*debug_tt32_tracing) (int);
#endif // defined(OPTION_W32_CTCI)

/* Function in crypto.c */
#ifdef FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_3
void renew_wrapping_keys(void);
#endif

#endif // _HEXTERNS_H
