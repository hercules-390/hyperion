/* SES.C        (c) Copyright Jan Jaeger, 1999-2001                  */
/*              Sysplex Instructions                                 */

/* z/Architecture support - (c) Copyright Jan Jaeger, 1999-2001      */

#include "hercules.h"

#include "opcode.h"

#if defined(FEATURE_STRUCTURED_EXTERNAL_STORAGE)
/*-------------------------------------------------------------------*/
/* 0105 CMSG  - ?                                                [E] */
/*-------------------------------------------------------------------*/
DEF_INST(ses_opcode_0105)
{
    E(inst, execflag, regs);

    SIE_ONLY_INSTRUCTION(regs);

    PRIV_CHECK(regs);

    SIE_INTERCEPT(regs);

}


/*-------------------------------------------------------------------*/
/* 0106 TMSG  - ?                                                [E] */
/*-------------------------------------------------------------------*/
DEF_INST(ses_opcode_0106)
{
    E(inst, execflag, regs);

    SIE_ONLY_INSTRUCTION(regs);

    PRIV_CHECK(regs);

    SIE_INTERCEPT(regs);

}


/*-------------------------------------------------------------------*/
/* 0108 TMPS  - ?                                                [E] */
/*-------------------------------------------------------------------*/
DEF_INST(ses_opcode_0108)
{
    E(inst, execflag, regs);

    SIE_ONLY_INSTRUCTION(regs);

    PRIV_CHECK(regs);

    SIE_INTERCEPT(regs);

}


/*-------------------------------------------------------------------*/
/* 0109 CMPS  - ?                                                [E] */
/*-------------------------------------------------------------------*/
DEF_INST(ses_opcode_0109)
{
    E(inst, execflag, regs);

    SIE_ONLY_INSTRUCTION(regs);

    PRIV_CHECK(regs);

    SIE_INTERCEPT(regs);

}


/*-------------------------------------------------------------------*/
/* B260 ????? - Move Channel Buffer Data                       [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(ses_opcode_B260)
{
int r1, r2;                                     /* register values   */

    RRE(inst, execflag, regs, r1, r2);

    SIE_ONLY_INSTRUCTION(regs);

    PRIV_CHECK(regs);

    SIE_INTERCEPT(regs);

}


/*-------------------------------------------------------------------*/
/* B261 ????? - Signal Channel Buffer                          [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(ses_opcode_B261)
{
int r1, r2;                                     /* register values   */

    RRE(inst, execflag, regs, r1, r2);

    SIE_ONLY_INSTRUCTION(regs);

    PRIV_CHECK(regs);

    SIE_INTERCEPT(regs);

}


/*-------------------------------------------------------------------*/
/* B264 ????? - TS                                             [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(ses_opcode_B264)
{
int r1, r2;                                     /* register values   */

    RRE(inst, execflag, regs, r1, r2);

    SIE_ONLY_INSTRUCTION(regs);

    PRIV_CHECK(regs);

    SIE_INTERCEPT(regs);

}


/*-------------------------------------------------------------------*/
/* B265 ????? - RS                                             [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(ses_opcode_B265)
{
int r1, r2;                                     /* register values   */

    RRE(inst, execflag, regs, r1, r2);

    SIE_ONLY_INSTRUCTION(regs);

    PRIV_CHECK(regs);

    SIE_INTERCEPT(regs);

}


/*-------------------------------------------------------------------*/
/* B266 ????? - TV                                             [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(ses_opcode_B266)
{
int r1, r2;                                     /* register values   */

    RRE(inst, execflag, regs, r1, r2);

    SIE_ONLY_INSTRUCTION(regs);

    PRIV_CHECK(regs);

    SIE_INTERCEPT(regs);

}


/*-------------------------------------------------------------------*/
/* B267 ????? - SV                                             [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(ses_opcode_B267)
{
int r1, r2;                                     /* register values   */

    RRE(inst, execflag, regs, r1, r2);

    SIE_ONLY_INSTRUCTION(regs);

    PRIV_CHECK(regs);

    SIE_INTERCEPT(regs);

}


/*-------------------------------------------------------------------*/
/* B268 ????? - Define Vector                                  [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(ses_opcode_B268)
{
int r1, r2;                                     /* register values   */

    RRE(inst, execflag, regs, r1, r2);

    SIE_ONLY_INSTRUCTION(regs);

    PRIV_CHECK(regs);

    SIE_INTERCEPT(regs);

}


/*-------------------------------------------------------------------*/
/* B272 ????? - Send Message ???                               [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(ses_opcode_B272)
{
int     b2;                             /* Base of effective addr    */
U32     effective_addr2;                /* Effective address         */

    S(inst, execflag, regs, b2, effective_addr2);

    SIE_ONLY_INSTRUCTION(regs);

    PRIV_CHECK(regs);

    SIE_INTERCEPT(regs);

}


/*-------------------------------------------------------------------*/
/* B27A ????? - Prepare Message Subchannel                     [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(ses_opcode_B27A)
{
int     b2;                             /* Base of effective addr    */
U32     effective_addr2;                /* Effective address         */

    S(inst, execflag, regs, b2, effective_addr2);

    SIE_ONLY_INSTRUCTION(regs);

    PRIV_CHECK(regs);

    SIE_INTERCEPT(regs);

}


/*-------------------------------------------------------------------*/
/* B27B ????? - Send Message Response                          [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(ses_opcode_B27B)
{
int     b2;                             /* Base of effective addr    */
U32     effective_addr2;                /* Effective address         */

    S(inst, execflag, regs, b2, effective_addr2);

    SIE_ONLY_INSTRUCTION(regs);

    PRIV_CHECK(regs);

    SIE_INTERCEPT(regs);

}


/*-------------------------------------------------------------------*/
/* B27C ????? - Send Secondary Message                         [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(ses_opcode_B27C)
{
int     b2;                             /* Base of effective addr    */
U32     effective_addr2;                /* Effective address         */

    S(inst, execflag, regs, b2, effective_addr2);

    SIE_ONLY_INSTRUCTION(regs);

    PRIV_CHECK(regs);

    SIE_INTERCEPT(regs);

}


/*-------------------------------------------------------------------*/
/* B27E ????? - PB                                             [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(ses_opcode_B27E)
{
int     b2;                             /* Base of effective addr    */
U32     effective_addr2;                /* Effective address         */

    S(inst, execflag, regs, b2, effective_addr2);

    SIE_ONLY_INSTRUCTION(regs);

    PRIV_CHECK(regs);

    SIE_INTERCEPT(regs);

}


/*-------------------------------------------------------------------*/
/* B27F ????? - Test Channel Buffer                            [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(ses_opcode_B27F)
{
int     b2;                             /* Base of effective addr    */
U32     effective_addr2;                /* Effective address         */

    S(inst, execflag, regs, b2, effective_addr2);

    SIE_ONLY_INSTRUCTION(regs);

    PRIV_CHECK(regs);

    SIE_INTERCEPT(regs);

}


/*-------------------------------------------------------------------*/
/* B2A4 ????? - Move Channel Buffer Data Multiple              [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(ses_opcode_B2A4)
{
int     b2;                             /* Base of effective addr    */
U32     effective_addr2;                /* Effective address         */

    S(inst, execflag, regs, b2, effective_addr2);

    SIE_ONLY_INSTRUCTION(regs);

    PRIV_CHECK(regs);

    SIE_INTERCEPT(regs);

}


/*-------------------------------------------------------------------*/
/* B2A8 ????? - ?                                              [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(ses_opcode_B2A8)
{
int     b2;                             /* Base of effective addr    */
U32     effective_addr2;                /* Effective address         */

    S(inst, execflag, regs, b2, effective_addr2);

    SIE_ONLY_INSTRUCTION(regs);

    PRIV_CHECK(regs);

    SIE_INTERCEPT(regs);

}


/*-------------------------------------------------------------------*/
/* B2F1 ????? - ?                                              [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(ses_opcode_B2F1)
{
int     b2;                             /* Base of effective addr    */
U32     effective_addr2;                /* Effective address         */

    S(inst, execflag, regs, b2, effective_addr2);

    SIE_ONLY_INSTRUCTION(regs);

    PRIV_CHECK(regs);

    SIE_INTERCEPT(regs);

}


/*-------------------------------------------------------------------*/
/* B2F6 ????? - WS                                             [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(ses_opcode_B2F6)
{
int     b2;                             /* Base of effective addr    */
U32     effective_addr2;                /* Effective address         */

    S(inst, execflag, regs, b2, effective_addr2);

    SIE_ONLY_INSTRUCTION(regs);

    PRIV_CHECK(regs);

    SIE_INTERCEPT(regs);

}
#endif /*defined(FEATURE_STRUCTURED_EXTERNAL_STORAGE)*/


#if !defined(_GEN_ARCH)

#define  _GEN_ARCH 390
#include "ses.c"

#undef   _GEN_ARCH
#define  _GEN_ARCH 370
#include "ses.c"

#endif /*!defined(_GEN_ARCH)*/
