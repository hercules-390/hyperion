/* HDL.H        (c) Copyright Jan Jaeger, 2003                       */
/*              Hercules Dynamic Loader                              */


#if !defined(_HDL_H)
#define _HDL_H

int hdl_load(char *);                 		/* load dll          */
int hdl_dele(char *);                           /* unload dll        */
void hdl_list();                          /* list all loaded modules */

void hdl_main();  

void * hdl_nent(char *, void*);
void * hdl_fent(char *);


#define HDL_RESO hdl_reso
#define HDL_INIT hdl_init
#define HDL_FINI hdl_fini

#define QSTRING(_string) STRINGMAC(_string)

#define HDL_RESO_Q QSTRING(HDL_RESO)
#define HDL_INIT_Q QSTRING(HDL_INIT)
#define HDL_FINI_Q QSTRING(HDL_FINI)


#define HDL_RESOLVER_SECTION                            \
void HDL_RESO(void *(*hdl_reso_fent)(char *) __attribute__ ((unused)) ) \
{

#define HDL_RESOLVE(_name)                              \
    (_name) = (hdl_reso_fent)(STRINGMAC(_name))

#define END_RESOLVER_SECTION                            \
}


#define HDL_REGISTER_SECTION                            \
void HDL_INIT(int (*hdl_init_regi)(char *, void *) __attribute__ ((unused)) ) \
{

#define HDL_REGISTER(_name, _ep)                        \
    (hdl_init_regi)(STRINGMAC(_name),&(_ep))

#define END_REGISTER_SECTION                            \
}

#define HDL_FINDENT(_name)                              \
    hdl_fent( (_name) )

#define HDL_FINDNEXT(_name, _ep)                        \
    hdl_nent( STRINGMAC(_name), &(_ep) )

#define HDL_FINAL_SECTION                               \
void HDL_FINI()                                         \
{

#define END_FINAL_SECTION                               \
}


struct _MODENT; 
struct _DLLENT;


typedef struct _MODENT {
    void (*fep)();                           /* Function entry point */
    char *name;                                     /* Function name */
    int  count;
    struct _DLLENT *dllent;
    struct _MODENT *modnext;
} MODENT;


typedef struct _DLLENT {
    char *name;
    void *dll;
    int type;
#define DLL_TYPE_MAIN 0
#define DLL_TYPE_LOAD 1
    int (*hdlreso)(void *);                  /* hdl_reso             */
    int (*hdlinit)(void *);                  /* hdl_init             */
    int (*hdlfini)();                        /* hdl_fini             */
    struct _MODENT *modent;
    struct _DLLENT *dllnext;
} DLLENT;

#endif
