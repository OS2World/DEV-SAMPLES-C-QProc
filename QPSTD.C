/*****************************************************************/
/**		     Microsoft LAN Manager			**/
/**	       Copyright(c) Microsoft Corp., 1985-1990		**/
/*****************************************************************/
/****************************** Module Header ******************************\

Module Name: QPSTD.C

This module contains the code for printing a PM_Q_STD format spooler file
(i.e. a GPI Metafile).

History:
 07-Sep-88 [stevewo]  Created.

\***************************************************************************/

#define INCL_GPICONTROL
#define INCL_GPITRANSFORMS
#define INCL_GPIMETAFILES
#define INCL_GPIREGIONS
#define INCL_DEV
#include "pmprint.h"

BOOL PASCAL SplQpStdPrintFile( pQProc, pszFileName )
register PQPROCINST pQProc;
PSZ pszFileName;
{
    BOOL result = FALSE;
    HDC  hdcDisplay;
    PSZ  devOpenData[ DRIVER_NAME+1 ];
    LONG pmfOptions[ PMF_COLORREALIZABLE+1 ];
    SIZEL ptSize;
    HRGN Region;

    SplOutSem();
    pQProc->hmf = NULL;
    hdcDisplay = NULL;
    if (OpenQPInputFile( pQProc, pszFileName, FALSE ) &&
       (pQProc->hmf = (HMF)GpiLoadMetaFile( HABX, pszFileName ))) {
        devOpenData[ ADDRESS ] = pQProc->pszPortName;
        devOpenData[ DRIVER_NAME ] = pQProc->pszDriverName;
        devOpenData[ DRIVER_DATA ] = (PSZ)pQProc->pDriverData;
        pQProc->hInfodc = DevOpenDC( HABX, OD_INFO, "*", (LONG)DRIVER_DATA+1,
                                devOpenData, pQProc->hInfodc );

        ptSize.cx = 0;
        ptSize.cy = 0;
        if (pQProc->hps = GpiCreatePS( HABX, pQProc->hInfodc,
                                         (PSIZEL)&ptSize,
                                         PU_PELS | GPIA_ASSOC )) {
            pmfOptions[ PMF_SEGBASE         ] = 0L;
            pmfOptions[ PMF_LOADTYPE        ] = LT_ORIGINALVIEW;
            pmfOptions[ PMF_RESOLVE         ] = RS_DEFAULT;
            pmfOptions[ PMF_LCIDS           ] = LC_LOADDISC;
            pmfOptions[ PMF_RESET           ] = RES_RESET;
            pmfOptions[ PMF_SUPPRESS        ] = SUP_SUPPRESS;
            pmfOptions[ PMF_COLORTABLES     ] = 0;
            pmfOptions[ PMF_COLORREALIZABLE ] = 0;
            GpiPlayMetaFile( pQProc->hps, pQProc->hmf,
                             (LONG)PMF_SUPPRESS+1, pmfOptions,
                             NULL, 0L, NULL );

            GpiAssociate( pQProc->hps, NULL );

            DevCloseDC( pQProc->hInfodc );        /* If any of this fails you
                                                     are in deep shit. */

            OpenQPOutputDC(pQProc, ASSOCIATE);

            result = !pQProc->qparms.fTransform || SetViewMatrix( pQProc );

            }

        pmfOptions[ PMF_SEGBASE         ] = 0L;
        pmfOptions[ PMF_LOADTYPE        ] = LT_ORIGINALVIEW;
        pmfOptions[ PMF_RESOLVE         ] = RS_DEFAULT;
        pmfOptions[ PMF_LCIDS           ] = LC_LOADDISC;
        pmfOptions[ PMF_RESET           ] = RES_NORESET;
        pmfOptions[ PMF_SUPPRESS        ] = SUP_NOSUPPRESS;
        pmfOptions[ PMF_COLORTABLES     ] = CTAB_REPLACE;
        pmfOptions[ PMF_COLORREALIZABLE ] = CREA_NOREALIZE;
        pmfOptions[ PMF_DEFAULTS        ] = DDEF_LOADDISC;

        if (GpiPlayMetaFile( pQProc->hps, pQProc->hmf,
                             (LONG)PMF_DEFAULTS+1, pmfOptions,
                             NULL, 0L, NULL ) != GPI_OK)
            if (pQProc->fsStatus & QP_ABORTED)
                result = FALSE;

        /* clear STOP_DRAW condition if job got aborted via other thread */
        GpiSetStopDraw( pQProc->hps, SDW_OFF );

        DevEscape(pQProc->hdc, DEVESC_ENDDOC, 0L, (PBYTE)NULL,
                  (PLONG)NULL, (PBYTE)NULL);

         if (pQProc->region) {
            GpiSetClipRegion(pQProc->hps, pQProc->region, &Region);
            GpiDestroyRegion(pQProc->hps,Region);
         }
        GpiAssociate( pQProc->hps, (HDC)0 );
        }

    if (pQProc->hmf) {
        GpiDeleteMetaFile( pQProc->hmf );
        pQProc->hmf = NULL;
        }

    if (pQProc->hps) {
        GpiDestroyPS( pQProc->hps );
        pQProc->hps = NULL;
        }

    CloseQPOutputDC( pQProc, FALSE);
    CloseQPInputFile( pQProc );

    if (!result)
        SplPanic( "SplQpStdPrintFile failed for %0s", pszFileName, 0 );

    return( result );
}


/* SetViewMatrix
 *
 * in:  pQProc - -> qproc instance struct
 * out: ok?
 */
BOOL SetViewMatrix( pQProc )
register PQPROCINST pQProc;
{
    SIZEL ptSize;
    RECTL ClipRectl, PageView;
    POINTL  Centre,l_t_in_pels,shift_value;
    BOOL Clip=FALSE;

    /*
      Not Implemented
    */

    return TRUE;
}
