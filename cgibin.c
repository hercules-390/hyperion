/* CGIBIN.C     (c)Copyright Jan Jaeger, 2002                        */
/*              HTTP cgi-bin routines                                */

/* This file contains all cgi routines that may be executed on the   */
/* server (ie under control of a hercules thread)                    */
/*                                                                   */
/*                                                                   */
/* All cgi-bin routines are identified in the directory at the end   */
/* of this file                                                      */
/*                                                                   */
/*                                                                   */
/* The cgi-bin routines may call the following HTTP service routines */
/*                                                                   */
/* char *cgi_variable(WEBBLK *webblk, char *name);                   */
/*   This call returns a pointer to the cgi variable requested       */
/*   or a NULL pointer if the variable is not found                  */
/*                                                                   */
/* char *cgi_username(WEBBLK *webblk);                               */
/*   Returns the username for which the user has been authenticated  */
/*   or NULL if not authenticated (refer to auth/noauth parameter    */
/*   on the HTTPPORT configuration statement)                        */
/*                                                                   */
/* char *cgi_baseurl(WEBBLK *webblk);                                */
/*   Returns the url as requested by the user                        */
/*                                                                   */
/*                                                                   */
/* void html_header(WEBBLK *webblk);                                 */
/*   Sets up the standard html header, and includes the              */
/*   html/header.htmlpart file.                                      */
/*                                                                   */
/* void html_footer(WEBBLK *webblk);                                 */
/*   Sets up the standard html footer, and includes the              */
/*   html/footer.htmlpart file.                                      */
/*                                                                   */
/* int html_include(WEBBLK *webblk, char *filename);                 */
/*   Includes an html file                                           */
/*                                                                   */
/*                                                                   */
/*                                           Jan Jaeger - 28/03/2002 */
 

#include "hercules.h"
#include "httpmisc.h"

#if defined(OPTION_HTTP_SERVER)


zz_cgibin cgibin_reg_control(WEBBLK *webblk)
{
int i;

    REGS *regs = sysblk.regs + sysblk.pcpu;

    html_header(webblk);

    fprintf(webblk->hsock, "<H2>Control Registers</H2>\n");
    fprintf(webblk->hsock, "<PRE>\n");
    if(regs->arch_mode != ARCH_900)
        for (i = 0; i < 16; i++)
            fprintf(webblk->hsock, "CR%2.2d=%8.8X%s", i, regs->CR_L(i),
                ((i & 0x03) == 0x03) ? "\n" : "\t");
    else
        for (i = 0; i < 16; i++)
            fprintf(webblk->hsock, "CR%1.1X=%16.16llX%s", i, regs->CR_G(i),
                ((i & 0x03) == 0x03) ? "\n" : " ");           

    fprintf(webblk->hsock, "</PRE>\n");

    html_footer(webblk);

    return NULL;
}


zz_cgibin cgibin_reg_general(WEBBLK *webblk)
{
int i;

    REGS *regs = sysblk.regs + sysblk.pcpu;

    html_header(webblk);

    fprintf(webblk->hsock, "<H2>General Registers</H2>\n");
    fprintf(webblk->hsock, "<PRE>\n");
    if(regs->arch_mode != ARCH_900)
        for (i = 0; i < 16; i++)
            fprintf(webblk->hsock, "GR%2.2d=%8.8X%s", i, regs->GR_L(i),
                ((i & 0x03) == 0x03) ? "\n" : "\t");
    else
        for (i = 0; i < 16; i++)
            fprintf(webblk->hsock, "GR%1.1X=%16.16llX%s", i, regs->GR_G(i),
                ((i & 0x03) == 0x03) ? "\n" : " ");           

    fprintf(webblk->hsock, "</PRE>\n");

    html_footer(webblk);

    return NULL;
}


void store_psw (REGS *regs, BYTE *addr);

zz_cgibin cgibin_psw(WEBBLK *webblk)
{
    REGS *regs = sysblk.regs + sysblk.pcpu;
    QWORD   qword;                            /* quadword work area      */  

    char *value;
    int autorefresh=0;
    int refresh_interval=5;

    html_header(webblk);


    if (cgi_variable(webblk,"autorefresh"))
        autorefresh = 1;
    else if (cgi_variable(webblk,"norefresh"))
        autorefresh = 0;
    else if (cgi_variable(webblk,"refresh"))
        autorefresh = 1;

    if ((value = cgi_variable(webblk,"refresh_interval")))
        refresh_interval = atoi(value);

    fprintf(webblk->hsock, "<H2>Program Status Word</H2>\n");

    fprintf(webblk->hsock, "<FORM method=post>\n");

    if (!autorefresh)
    {
        fprintf(webblk->hsock, "<INPUT type=submit value=\"Auto Refresh\" name=autorefresh>\n");
        fprintf(webblk->hsock, "Refresh Interval: ");
        fprintf(webblk->hsock, "<INPUT type=text size=2 name=\"refresh_interval\" value=%d>\n", 
           refresh_interval);
    }
    else
    {
        fprintf(webblk->hsock, "<INPUT type=submit value=\"Stop Refreshing\" name=norefresh>\n");
        fprintf(webblk->hsock, "Refresh Interval: %d\n", refresh_interval);
        fprintf(webblk->hsock, "<INPUT type=hidden name=\"refresh_interval\" value=%d>\n",refresh_interval);
    }

    fprintf(webblk->hsock, "</FORM>\n");

    fprintf(webblk->hsock, "<P>\n");

    if( regs->arch_mode != ARCH_900 )
    {
        store_psw (regs, qword);
        fprintf(webblk->hsock, "PSW=%2.2X%2.2X%2.2X%2.2X %2.2X%2.2X%2.2X%2.2X\n",
                qword[0], qword[1], qword[2], qword[3],
                qword[4], qword[5], qword[6], qword[7]);
    }
    else
    {
        store_psw (regs, qword);
        fprintf(webblk->hsock, "PSW=%2.2X%2.2X%2.2X%2.2X %2.2X%2.2X%2.2X%2.2X "
                "%2.2X%2.2X%2.2X%2.2X%2.2X%2.2X%2.2X%2.2X\n",
                qword[0], qword[1], qword[2], qword[3],
                qword[4], qword[5], qword[6], qword[7],
                qword[8], qword[9], qword[10], qword[11],
                qword[12], qword[13], qword[14], qword[15]);
    }
 
    if (autorefresh)
    {
        /* JavaScript to cause automatic page refresh */
        fprintf(webblk->hsock, "<script language=\"JavaScript\">\n");
        fprintf(webblk->hsock, "<!--\nsetTimeout('window.location.replace(\"%s?refresh_interval=%d&refresh=1\")', %d)\n", 
               cgi_baseurl(webblk),
               refresh_interval,
               refresh_interval*1000);
        fprintf(webblk->hsock, "//-->\n</script>\n");
    }

    html_footer(webblk);

    return NULL;
}


void get_msgbuf(BYTE **msgbuf, int *msgslot, int *nummsgs, int *msg_size, int *max_msgs);

zz_cgibin cgibin_syslog(WEBBLK *webblk)
{
BYTE   *msgbuf;                         /* Circular message buffer   */
int     msgslot;                        /* Next available buffer slot*/
int     nummsgs;                        /* Number of msgs in buffer  */
int     msg_size;
int     max_msgs;
BYTE   *pointer;
int     currmsgn;
char   *command;

char *value;
int autorefresh = 0;
int refresh_interval = 5;
int msgcount = 0;

    if ((command = cgi_variable(webblk,"command")))
        panel_command(command);

    if((value = cgi_variable(webblk,"msgcount")))
        msgcount = atoi(value);

    if ((value = cgi_variable(webblk,"refresh_interval")))
        refresh_interval = atoi(value);

    if (cgi_variable(webblk,"autorefresh"))
        autorefresh = 1;
    else if (cgi_variable(webblk,"norefresh"))
        autorefresh = 0;
    else if (cgi_variable(webblk,"refresh"))
        autorefresh = 1;

    html_header(webblk);

    fprintf(webblk->hsock, "<H2>Hercules System Log</H2>\n");
    fprintf(webblk->hsock, "<PRE>\n");

    get_msgbuf(&msgbuf, &msgslot, &nummsgs, &msg_size, &max_msgs);

    for(currmsgn = 0; currmsgn < nummsgs; currmsgn++) {
        if(!msgcount || currmsgn >= (nummsgs - msgcount))
        {
            if(nummsgs < max_msgs)
                pointer = msgbuf + (currmsgn * msg_size);
            else
                if(currmsgn + msgslot < max_msgs) 
                    pointer = msgbuf + ((currmsgn + msgslot) * msg_size);
                else
                    pointer = msgbuf + ((currmsgn + msgslot - max_msgs) * msg_size);
    
            fprintf(webblk->hsock,"%80.80s\n",pointer);
        }
    }

    fprintf(webblk->hsock, "</PRE>\n");

    fprintf(webblk->hsock, "<FORM method=post>Command:\n");
    fprintf(webblk->hsock, "<INPUT type=text name=command size=80>\n");
    fprintf(webblk->hsock, "<INPUT type=hidden name=%srefresh value=1>\n",autorefresh ? "auto" : "no");
    fprintf(webblk->hsock, "<INPUT type=hidden name=refresh_interval value=%d>\n",refresh_interval);
    fprintf(webblk->hsock, "<INPUT type=hidden name=msgcount value=%d>\n",msgcount);
    fprintf(webblk->hsock, "</FORM>\n<BR>\n");

    fprintf(webblk->hsock, "<A name=bottom>\n");

    fprintf(webblk->hsock, "<FORM method=post>\n");
    if(!autorefresh)
    {
        fprintf(webblk->hsock, "<INPUT type=submit value=\"Auto Refresh\" name=autorefresh>\n");
        fprintf(webblk->hsock, "Refresh Interval: ");
        fprintf(webblk->hsock, "<INPUT type=text name=\"refresh_interval\" size=2 value=%d>\n", 
           refresh_interval);
    }
    else
    {
        fprintf(webblk->hsock, "<INPUT type=submit name=norefresh value=\"Stop Refreshing\">\n");
        fprintf(webblk->hsock, "<INPUT type=hidden name=refresh_interval value=%d>\n",refresh_interval);
        fprintf(webblk->hsock, " Refresh Interval: %2d \n", refresh_interval);
    }
    fprintf(webblk->hsock, "<INPUT type=hidden name=msgcount value=%d>\n",msgcount);
    fprintf(webblk->hsock, "</FORM>\n");

    fprintf(webblk->hsock, "<FORM method=post>\n");
    fprintf(webblk->hsock, "Only show last ");
    fprintf(webblk->hsock, "<INPUT type=text name=msgcount size=3 value=%d>",msgcount);
    fprintf(webblk->hsock, " lines (zero for all loglines)\n");
    fprintf(webblk->hsock, "<INPUT type=hidden name=%srefresh value=1>\n",autorefresh ? "auto" : "no");
    fprintf(webblk->hsock, "<INPUT type=hidden name=refresh_interval value=%d>\n",refresh_interval);
    fprintf(webblk->hsock, "</FORM>\n");
    
    if (autorefresh)
    {
        /* JavaScript to cause automatic page refresh */
        fprintf(webblk->hsock, "<script language=\"JavaScript\">\n");
        fprintf(webblk->hsock, "<!--\nsetTimeout('window.location.replace(\"%s"
               "?refresh_interval=%d"
               "&refresh=1"
               "&msgcount=%d"
               "\")', %d)\n", 
               cgi_baseurl(webblk),
               refresh_interval,
               msgcount,
               refresh_interval*1000);
        fprintf(webblk->hsock, "//-->\n</script>\n");
    }

    html_footer(webblk);

    return NULL;
}


/* The following table is the cgi-bin directory, which               */
/* associates directory filenames with cgibin routines               */

CGITAB cgidir[] = {
    { "syslog", (void*)&cgibin_syslog },
    { "registers/general", (void*)&cgibin_reg_general },
    { "registers/control", (void*)&cgibin_reg_control },
    { "registers/psw", (void*)&cgibin_psw },
    { NULL, NULL } };

#endif /*defined(OPTION_HTTP_SERVER)*/
