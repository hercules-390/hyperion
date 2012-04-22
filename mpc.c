/* MPC.C        (c) Copyright Jan Jaeger,  2010-2012                 */
/*              (c) Copyright Ian Shorter, 2011-2012                 */
/*              (c) Copyright Harold Grovesteen, 2011-2012           */
/*              MPC                                                  */

/* This implementation is based on the S/390 Linux implementation    */

#include "hstdinc.h"

#if !defined(_HENGINE_DLL_)
#define _HENGINE_DLL_
#endif

#if !defined(_MPC_C_)
#define _MPC_C_
#endif

#include "hercules.h"

#include "mpc.h"


/*--------------------------------------------------------------------*/
/* point_ipa():                                                       */
/*--------------------------------------------------------------------*/
MPC_IPA*  point_ipa( DEVBLK* pDEVBLK, MPC_TH* pMPC_TH, MPC_RRH* pMPC_RRH )
{
    UNREFERENCED( pDEVBLK );

    MPC_PH*    pMPC_PH;
    MPC_IPA*   pMPC_IPA;
    U32        uOffData;
    U16        uOffPH;

    // Point to the MPC_PH.
    FETCH_HW( uOffPH, pMPC_RRH->offph );
    pMPC_PH = (MPC_PH*)((BYTE*)pMPC_RRH + uOffPH);

    // Get the length of and point to the data referenced by the
    // MPC_PH. The data contain a MPC_IPA.
    FETCH_FW( uOffData, pMPC_PH->offdata );
    pMPC_IPA = (MPC_IPA*)((BYTE*)pMPC_TH + uOffData);

    return pMPC_IPA;
}   /* End function  point_ipa() */

/*--------------------------------------------------------------------*/
/* point_puk():                                                       */
/*--------------------------------------------------------------------*/
MPC_PUK*  point_puk( DEVBLK* pDEVBLK, MPC_TH* pMPC_TH, MPC_RRH* pMPC_RRH )
{
    UNREFERENCED( pDEVBLK );

    MPC_PH*    pMPC_PH;
    MPC_PUK*   pMPC_PUK;
    U32        uOffData;
    U16        uOffPH;

    // Point to the MPC_PH.
    FETCH_HW( uOffPH, pMPC_RRH->offph );
    pMPC_PH = (MPC_PH*)((BYTE*)pMPC_RRH + uOffPH);

    // Get the length of and point to the data referenced by the
    // MPC_PH. The data contain a MPC_PUK and one or more MPC_PUSs.
    FETCH_FW( uOffData, pMPC_PH->offdata );
    pMPC_PUK = (MPC_PUK*)((BYTE*)pMPC_TH + uOffData);

    return pMPC_PUK;
}   /* End function  point_puk() */

/*--------------------------------------------------------------------*/
/* point_pus():                                                       */
/*--------------------------------------------------------------------*/
/* Return a pointer to the MPC_PUS of the required type. If the       */
/* required type is not found a null pointer is returned. Although    */
/* never seen, if multiple MPC_PUS of the required type exist, only   */
/* the first MPC_PUS of the required type will ever be returned.      */
MPC_PUS*  point_pus( DEVBLK* pDEVBLK, MPC_PUK* pMPC_PUK, BYTE bType )
{
    UNREFERENCED( pDEVBLK );

    MPC_PUS*   pMPC_PUS;
    int        iTotLenPUS;
    U16        uTotLenPUS;
    U16        uLenPUS;
    U16        uLenPUK;

    /* Get the length of the MPC_PUK, the total length of the */
    /* following MPC_PUSs, then point to the first MPC_PUS.   */
    FETCH_HW( uLenPUK, pMPC_PUK->length );
    FETCH_HW( uTotLenPUS, pMPC_PUK->lenpus );
    iTotLenPUS = uTotLenPUS;
    pMPC_PUS = (MPC_PUS*)((BYTE*)pMPC_PUK + uLenPUK);

    /* Find the required MPC_PUS. */
    while( iTotLenPUS > 0 )
    {
        /* Check for the required MPC_PUS. */
        if( pMPC_PUS->type == bType )
            return pMPC_PUS;

        /* Get the length of the MPC_PUS.                            */
        /* (The length check is probably unnecessary, a zero length  */
        /* has never been seen. Better safe than sorry though.)      */
        FETCH_HW( uLenPUS, pMPC_PUS->length );
        if( uLenPUS == 0 )
            return NULL;
        iTotLenPUS -= uLenPUS;

        /* Point to the next MPC_PUS. */
        pMPC_PUS = (MPC_PUS*)((BYTE*)pMPC_PUS + uLenPUS);
    }

    return NULL;
}   /* End function  point_pus() */

/*--------------------------------------------------------------------*/
/* display_stuff()                                                    */
/*--------------------------------------------------------------------*/
/* Function to display storage.                                       */
void  display_stuff( DEVBLK* pDEVBLK, char* cWhat, BYTE* pAddr, int iLen, BYTE bDir )
{
    int           offset;
    unsigned int  i;
    u_char        c = '\0';
    u_char        e = '\0';
    char          print_ascii[17];
    char          print_ebcdic[17];
    char          print_line[64];
    char          tmp[32];

    for( offset = 0; offset < iLen; )
    {
        memset( print_ascii, ' ', sizeof(print_ascii)-1 );    /* set to spaces */
        print_ascii[sizeof(print_ascii)-1] = '\0';            /* with null termination */
        memset( print_ebcdic, ' ', sizeof(print_ebcdic)-1 );  /* set to spaces */
        print_ebcdic[sizeof(print_ebcdic)-1] = '\0';          /* with null termination */
        memset( print_line, 0, sizeof( print_line ) );

        snprintf( (char*)print_line, sizeof(print_line), "+%4.4X%c ", offset, bDir );
        print_line[sizeof(print_line)-1] = '\0';              /* force null termination */

        for( i = 0; i < 16; i++ )
        {
            c = *pAddr++;

            if( offset < iLen )
            {
                snprintf((char *) tmp, 32, "%2.2X", c );
                tmp[sizeof(tmp)-1] = '\0';
                strlcat((char *) print_line, (char *) tmp, sizeof(print_line) );

                print_ebcdic[i] = print_ascii[i] = '.';
                e = guest_to_host( c );

                if( isprint( e ) )
                    print_ebcdic[i] = e;
                if( isprint( c ) )
                    print_ascii[i] = c;
            }
            else
            {
                strlcat((char *) print_line, "  ", sizeof(print_line) );
            }

            offset++;
            if( ( offset & 3 ) == 0 )
            {
                strlcat((char *) print_line, " ", sizeof(print_line) );
            }
        }

        if( pDEVBLK )
        {
            // HHC03981 "%1d:%04X %s: %s: %s %s %s"
            WRMSG(HHC03981, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, pDEVBLK->typname,
                                 cWhat, print_line, print_ascii, print_ebcdic );
        }
        else
        {
            // HHC03982 "%s: %s %s %s"
            WRMSG(HHC03982, "I", cWhat, print_line, print_ascii, print_ebcdic );
        }
    }

    return;
}   /* End function  display_stuff() */

/*--------------------------------------------------------------------*/
/* display_th(): Display Transport Header (MPC_TH)                    */
/*--------------------------------------------------------------------*/
void  display_th( DEVBLK* pDEVBLK, MPC_TH* pMPC_TH, BYTE bDir )
{
    U32    uOffRRH;

    // Display the MPC_TH.
    FETCH_FW( uOffRRH, pMPC_TH->offrrh );
    display_stuff( pDEVBLK, "TH", (BYTE*)pMPC_TH, (int)uOffRRH, bDir );

    return;
}   /* End function  display_th() */

/*--------------------------------------------------------------------*/
/* display_rrh(): Display Request/Response Header (MPC_RRH)           */
/*--------------------------------------------------------------------*/
void  display_rrh( DEVBLK* pDEVBLK, MPC_RRH* pMPC_RRH, BYTE bDir )
{
    U16    uOffPH;

    // Display the MPC_RRH.
    FETCH_HW( uOffPH, pMPC_RRH->offph );
    display_stuff( pDEVBLK, "RRH", (BYTE*)pMPC_RRH, (int)uOffPH, bDir );

    return;
}   /* End function  display_rrh() */

/*--------------------------------------------------------------------*/
/* display_ph(): Display Protocol Data Unit Header (MPC_PH)           */
/*--------------------------------------------------------------------*/
void  display_ph( DEVBLK* pDEVBLK, MPC_PH* pMPC_PH, BYTE bDir )
{

    // Display the MPC_PH.
    display_stuff( pDEVBLK, "PH", (BYTE*)pMPC_PH, (int)SIZE_PH, bDir );

    return;
}   /* End function  display_ph() */

/*--------------------------------------------------------------------*/
/* display_rrh_and_puk():                                             */
/*--------------------------------------------------------------------*/
/* In all cases that have been seen, on both OSA and PTP, when        */
/* MPC_RRH->proto == PROTOCOL_UNKNOWN (0x7E), the MPC_RRH is followed */
/* by a single MPC_PH, which is followed by a single MPC_PUK, which   */
/* is followed by up to four MPC_PUSs.                                */
void  display_rrh_and_puk( DEVBLK* pDEVBLK, MPC_TH* pMPC_TH, MPC_RRH* pMPC_RRH, char* pDesc, BYTE bDir )
{
    MPC_PH*    pMPC_PH;
    MPC_PUK*   pMPC_PUK;
    MPC_PUS*   pMPC_PUS;
    int        iTotLenPUS;
    U32        uOffData;
    U16        uTotLenPUS;
    U16        uLenPUS;
    U16        uLenPUK;
    U16        uOffPH;

    /* Display description, if one has been provided. */
    if( pDesc )
    {
        if( pDEVBLK )
        {
            // HHC03983 "%1d:%04X %s: %s
            WRMSG(HHC03983, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, pDEVBLK->typname, pDesc );
        }
        else
        {
            // HHC03984 "%s"
            WRMSG(HHC03984, "I", pDesc );
        }
    }

    // Display the MPC_RRH.
    FETCH_HW( uOffPH, pMPC_RRH->offph );
    display_stuff( pDEVBLK, "RRH", (BYTE*)pMPC_RRH, (int)uOffPH, bDir );

    // Point to and display the MPC_PH.
    pMPC_PH = (MPC_PH*)((BYTE*)pMPC_RRH + uOffPH);
    display_stuff( pDEVBLK, "PH", (BYTE*)pMPC_PH, (int)SIZE_PH, bDir );

    // Get the length of and point to the data referenced by the
    // MPC_PH. The data contain a MPC_PUK and one or more MPC_PUSs.
    FETCH_FW( uOffData, pMPC_PH->offdata );
    pMPC_PUK = (MPC_PUK*)((BYTE*)pMPC_TH + uOffData);

    // Display the MPC_PUK.
    FETCH_HW( uLenPUK, pMPC_PUK->length );
    display_stuff( pDEVBLK, "PUK", (BYTE*)pMPC_PUK, (int)uLenPUK, bDir );

    // Get the total length of the following MPC_PUSs, then point to
    // the first MPC_PUS.
    FETCH_HW( uTotLenPUS, pMPC_PUK->lenpus );
    iTotLenPUS = uTotLenPUS;
    pMPC_PUS = (MPC_PUS*)((BYTE*)pMPC_PUK + uLenPUK);

    // Display all of the MPC_PUSs.
    while( iTotLenPUS > 0 )
    {
        // Get the length of the MPC_PUS.
        FETCH_HW( uLenPUS, pMPC_PUS->length );
        if( uLenPUS == 0 )                     /* Better safe than sorry */
            break;

        // Display the MPC_PUS.
        display_stuff( pDEVBLK, "PUS", (BYTE*)pMPC_PUS, (int)uLenPUS, bDir );

        // Point to the next MPC_PUS
        pMPC_PUS = (MPC_PUS*)((BYTE*)pMPC_PUS + uLenPUS);

        //
        iTotLenPUS -= uLenPUS;
    }

    return;
}   /* End function  display_rrh_and_puk() */

/*--------------------------------------------------------------------*/
/* display_rrh_and_pix():                                             */
/*--------------------------------------------------------------------*/
/* In all cases that have been seen, only on PTP, when MPC_RRH->type  */
/* == RRH_TYPE_IPA (0xC1) and MPC_RRH->proto == PROTOCOL_LAYER2       */
/* (0x08), the MPC_RRH is followed by a single MPC_PH, which is       */
/* followed by a single MPC_PIX.                                      */
void  display_rrh_and_pix( DEVBLK* pDEVBLK, MPC_TH* pMPC_TH, MPC_RRH* pMPC_RRH, char* pDesc, BYTE bDir )
{
    MPC_PH*    pMPC_PH;
    MPC_PIX*   pMPC_PIX;
    U32        uOffData;
    U16        uLenData;
    U16        uOffPH;

    /* Display description, if one has been provided. */
    if( pDesc )
    {
        if( pDEVBLK )
        {
            // HHC03983 "%1d:%04X %s: %s
            WRMSG(HHC03983, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, pDEVBLK->typname, pDesc );
        }
        else
        {
            // HHC03984 "%s"
            WRMSG(HHC03984, "I", pDesc );
        }
    }

    // Display the MPC_RRH.
    FETCH_HW( uOffPH, pMPC_RRH->offph );
    display_stuff( pDEVBLK, "RRH", (BYTE*)pMPC_RRH, (int)uOffPH, bDir );

    // Point to and display the MPC_PH.
    pMPC_PH = (MPC_PH*)((BYTE*)pMPC_RRH + uOffPH);
    display_stuff( pDEVBLK, "PH", (BYTE*)pMPC_PH, (int)SIZE_PH, bDir );

    // Point to and display the MPC_PIX.
    FETCH_HW( uLenData, pMPC_PH->lendata );
    FETCH_FW( uOffData, pMPC_PH->offdata );
    pMPC_PIX = (MPC_PIX*)((BYTE*)pMPC_TH + uOffData);
    display_stuff( pDEVBLK, "PIX", (BYTE*)pMPC_PIX, (int)uLenData, bDir );

    return;
}   /* End function  display_rrh_and_pix() */

/*--------------------------------------------------------------------*/
/* display_rrh_and_ipa():                                             */
/*--------------------------------------------------------------------*/
void  display_rrh_and_ipa( DEVBLK* pDEVBLK, MPC_TH* pMPC_TH, MPC_RRH* pMPC_RRH, char* pDesc, BYTE bDir )
{
    MPC_PH*    pMPC_PH;
    MPC_IPA*   pMPC_IPA;
    BYTE*      pMPC_IPA_CMD;
    U32        uOffData;
    U16        uLenData;
    U16        uOffPH;
    int        iLenIPA;
    int        iLenCmd;

    /* Display description, if one has been provided. */
    if( pDesc )
    {
        if( pDEVBLK )
        {
            // HHC03983 "%1d:%04X %s: %s
            WRMSG(HHC03983, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, pDEVBLK->typname, pDesc );
        }
        else
        {
            // HHC03984 "%s"
            WRMSG(HHC03984, "I", pDesc );
        }
    }

    // Display the MPC_RRH.
    FETCH_HW( uOffPH, pMPC_RRH->offph );
    display_stuff( pDEVBLK, "RRH", (BYTE*)pMPC_RRH, (int)uOffPH, bDir );

    // Point to and display the MPC_PH.
    pMPC_PH = (MPC_PH*)((BYTE*)pMPC_RRH + uOffPH);
    display_stuff( pDEVBLK, "PH", (BYTE*)pMPC_PH, (int)SIZE_PH, bDir );

    /* Point to and display the MPC_IPA (and commands, if any). */
    FETCH_HW( uLenData, pMPC_PH->lendata );
    if( uLenData > sizeof(MPC_IPA) )
    {
        iLenIPA = sizeof(MPC_IPA);
        iLenCmd = uLenData - sizeof(MPC_IPA);
    }
    else
    {
        iLenIPA = uLenData;
        iLenCmd = 0;
    }
    FETCH_FW( uOffData, pMPC_PH->offdata );
    pMPC_IPA = (MPC_IPA*)((BYTE*)pMPC_TH + uOffData);
    display_stuff( pDEVBLK, "IPA", (BYTE*)pMPC_IPA, iLenIPA, bDir );
    if( iLenCmd )
    {
        pMPC_IPA_CMD = (BYTE*)pMPC_IPA + iLenIPA;
        display_stuff( pDEVBLK, "Cmd", (BYTE*)pMPC_IPA_CMD, iLenCmd, bDir );
    }

    return;
}   /* End function  display_rrh_and_ipa() */

/*--------------------------------------------------------------------*/
/* display_rrh_and_pdu():                                             */
/*--------------------------------------------------------------------*/
/* In all cases that have been seen, only on PTP, when MPC_RRH->type  */
/* == RRH_TYPE_CM (0x81) and MPC_RRH->proto == PROTOCOL_LAYER2        */
/* (0x08), the MPC_RRH is followed by one or more MPC_PH, which are   */
/* followed by IP packet data. The function is not restricted to the  */
/* narrow case described above however, it is intended to be general  */
/* purpose. (Calls to display_rrh_and_puk() or display_rrh_and_pix()  */
/* could be replaced with calls to display_rrh_and_pdu(), with a      */
/* slight loss of functionality.)                                     */
void  display_rrh_and_pdu( DEVBLK* pDEVBLK, MPC_TH* pMPC_TH, MPC_RRH* pMPC_RRH, char* pDesc, BYTE bDir, int iLimit )
{
    MPC_PH*    pMPC_PH;
    U16        uNumPH;
    U16        uOffPH;
    int        iForPH;
    int        iDone;
    U16        uLenData;
    U32        uOffData;
    BYTE*      pData;

    /* Display description, if one has been provided. */
    if( pDesc )
    {
        if( pDEVBLK )
        {
            // HHC03983 "%1d:%04X %s: %s
            WRMSG(HHC03983, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, pDEVBLK->typname, pDesc );
        }
        else
        {
            // HHC03984 "%s"
            WRMSG(HHC03984, "I", pDesc );
        }
    }

    /* Display the MPC_RRH.*/
    FETCH_HW( uOffPH, pMPC_RRH->offph );
    display_stuff( pDEVBLK, "RRH", (BYTE*)pMPC_RRH, (int)uOffPH, bDir );

    /* Display the MPC_PH(s). */
    FETCH_HW( uNumPH, pMPC_RRH->numph );
    pMPC_PH = (MPC_PH*)((BYTE*)pMPC_RRH + uOffPH);
    for( iForPH = 1; iForPH <= uNumPH; iForPH++ )
    {
        display_stuff( pDEVBLK, "PH", (BYTE*)pMPC_PH, (int)SIZE_PH, bDir );
        pMPC_PH = (MPC_PH*)((BYTE*)pMPC_PH + SIZE_PH);
    }

    /* Display the data referenced by the MPC_PH(s).              */
    /* if limit is negative or a silly number, don't display the  */
    /* data. If limit is zero, display all of the data, otherwise */
    /* limit the length of the data displayed.                    */
    iDone = 0;
    if( iLimit >= 0 && iLimit <= 65535 )
    {
        pMPC_PH = (MPC_PH*)((BYTE*)pMPC_RRH + uOffPH);
        for( iForPH = 1; iForPH <= uNumPH; iForPH++ )
        {
            FETCH_HW( uLenData, pMPC_PH->lendata );
            FETCH_FW( uOffData, pMPC_PH->offdata );
            pData = (BYTE*)pMPC_TH + uOffData;
            if( iLimit > 0 )
            {
                if( iDone >= iLimit )
                    break;
                if( uLenData > ( iLimit - iDone ) )
                    uLenData = ( iLimit - iDone );
                iDone =+ uLenData;
            }
            display_stuff( pDEVBLK, "PDU", pData, (int)uLenData, bDir );
            pMPC_PH = (MPC_PH*)((BYTE*)pMPC_PH + SIZE_PH);
        }
    }

    return;
}   /* End function  display_rrh_and_pdu() */

/*--------------------------------------------------------------------*/
/* display_iea():                                                     */
/*--------------------------------------------------------------------*/
void  display_iea( DEVBLK* pDEVBLK, MPC_IEA* pMPC_IEA, char* pDesc, BYTE bDir )
{

    /* Display description, if one has been provided. */
    if( pDesc )
    {
        if( pDEVBLK )
        {
            // HHC03983 "%1d:%04X %s: %s
            WRMSG(HHC03983, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, pDEVBLK->typname, pDesc );
        }
        else
        {
            // HHC03984 "%s"
            WRMSG(HHC03984, "I", pDesc );
        }
    }

    /* Display MPC_IEA. */
    display_stuff( pDEVBLK, "IEA", (BYTE*)pMPC_IEA, (int)sizeof(MPC_IEA), bDir );

    return;
}   /* End function  display_iea() */

/*--------------------------------------------------------------------*/
/* display_iear():                                                    */
/*--------------------------------------------------------------------*/
void  display_iear( DEVBLK* pDEVBLK, MPC_IEAR* pMPC_IEAR, char* pDesc, BYTE bDir )
{

    /* Display description, if one has been provided. */
    if( pDesc )
    {
        if( pDEVBLK )
        {
            // HHC03983 "%1d:%04X %s: %s
            WRMSG(HHC03983, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, pDEVBLK->typname, pDesc );
        }
        else
        {
            // HHC03984 "%s"
            WRMSG(HHC03984, "I", pDesc );
        }
    }

    /* Display MPC_IEAR. */
    display_stuff( pDEVBLK, "IEAR", (BYTE*)pMPC_IEAR, (int)sizeof(MPC_IEAR), bDir );

    return;
}   /* End function  display_iear() */

/*--------------------------------------------------------------------*/
/* display_osa_th_etc():                                              */
/*--------------------------------------------------------------------*/
void  display_osa_th_etc( DEVBLK* pDEVBLK, MPC_TH* pMPC_TH, char* pDesc, BYTE bDir, int iLimit )
{
    MPC_RRH*   pMPC_RRH;
    int        iForRRH;
    U32        uOffRRH;
    U16        uNumRRH;

    /* Display description, if one has been provided. */
    if( pDesc )
    {
        if( pDEVBLK )
        {
            // HHC03983 "%1d:%04X %s: %s
            WRMSG(HHC03983, "I", SSID_TO_LCSS(pDEVBLK->ssid), pDEVBLK->devnum, pDEVBLK->typname, pDesc );
        }
        else
        {
            // HHC03984 "%s"
            WRMSG(HHC03984, "I", pDesc );
        }
    }

    /* Display MPC_TH. */
    display_th( pDEVBLK, pMPC_TH, bDir );

    /* Get the number of MPC_RRHs and the displacement from    */
    /* the start of the MPC_TH to the first (or only) MPC_RRH. */
    FETCH_HW( uNumRRH, pMPC_TH->numrrh );
    FETCH_FW( uOffRRH, pMPC_TH->offrrh );

    /* Process each of the MPC_RRHs. */
    for( iForRRH = 1; iForRRH <= uNumRRH; iForRRH++ )
    {

        /* Point to the first or subsequent MPC_RRH. */
        pMPC_RRH = (MPC_RRH*)((BYTE*)pMPC_TH + uOffRRH);

        /* Display the MPC_RRH etc. */
        if( pMPC_RRH->proto == PROTOCOL_UNKNOWN )
        {

            /* Display MPC_RRH and following MPC_PUK etc. */
            display_rrh_and_puk( pDEVBLK, pMPC_TH, pMPC_RRH, NULL, bDir );

        }
        else if( pMPC_RRH->proto == PROTOCOL_LAYER3 &&
                 pMPC_RRH->type == RRH_TYPE_IPA )
        {

            /* Display MPC_RRH and following MPC_IPA etc. */
            display_rrh_and_ipa( pDEVBLK, pMPC_TH, pMPC_RRH, NULL, bDir );

        }
        else
        {

            /* Display MPC_RRH and following PDU. */
            display_rrh_and_pdu( pDEVBLK, pMPC_TH, pMPC_RRH, NULL, bDir, iLimit );

        }

        /* Get the displacement from the start of the MPC_TH to the */
        /* next MPC_RRH. pMPC_RRH->offrrh will contain zero if this */
        /* is the last MPC_RRH.                                     */
        FETCH_FW( uOffRRH, pMPC_RRH->offrrh );

    }

    return;
}   /* End function  display_osa_th_etc() */

