/*****************************************************************/ 
/**		     Microsoft LAN Manager			**/ 
/**	       Copyright(c) Microsoft Corp., 1985-1990		**/ 
/*****************************************************************/ 
/****************************** Module Header ******************************\

Module Name: QPRAW.C

This module contains the code for printing a PM_Q_RAW format spooler file

History:
 07-Sep-88 [stevewo]  Created.

\***************************************************************************/

#define INCL_DEV

#include "pmprint.h"

#define BUFLEN 2048

BOOL PASCAL SplQpRawPrintFile( pQProc, pszFileName )
register PQPROCINST pQProc;
PSZ pszFileName;
{
    BOOL result = FALSE;
    ULONG  cbTotal;
    USHORT cb, cbRead;

    SplOutSem();
    pQProc->selBuf = NULL;
    if (OpenQPInputFile( pQProc, pszFileName, TRUE ) &&
        OpenQPOutputDC(pQProc, NOASSOC)) {

        if ((cbTotal = pQProc->cbFile) > BUFLEN)
            cb = BUFLEN;
        else
            cb = (USHORT)cbTotal;

        if (!DosAllocSeg( cb, &pQProc->selBuf, 0 )) {
            result = TRUE;
            while (cbTotal && !(pQProc->fsStatus & QP_ABORTED)) {
                if (cbTotal > BUFLEN)
                    cb = BUFLEN;
                else
                    cb = (USHORT)cbTotal;

                cbRead = 0;
                if (DosRead( pQProc->hFile, MAKEP( pQProc->selBuf, 0 ),
                             cb, (PUSHORT)&cbRead ) || cb != cbRead) {
                    pQProc->fsStatus |= QP_ABORTED;
                    pQProc->fsStatus &= ~ QP_PAUSED;
                    }
                else
                if (DevEscape( pQProc->hdc, DEVESC_RAWDATA, (LONG)cbRead,
                               MAKEP( pQProc->selBuf, 0 ),
                               (PLONG)NULL, (PBYTE)NULL ) == DEVESC_ERROR) {
                    pQProc->fsStatus |= QP_ABORTED;
                    pQProc->fsStatus &= ~ QP_PAUSED;
                    }
                else
                    cbTotal -= cbRead;

                if (pQProc->fsStatus & QP_PAUSED)
                    DosSemWait(&pQProc->semPaused,-1l);
                }

            if (pQProc->fsStatus & QP_ABORTED) {
                DevEscape( pQProc->hdc, DEVESC_ABORTDOC,
                           (LONG)0L, (PBYTE)NULL,
                           (PLONG)NULL, (PBYTE)NULL );
                result = TRUE;
                }
            else
            if (!cbTotal)
                result = TRUE;
            }
        }

    if (pQProc->selBuf) {
        DosFreeSeg( pQProc->selBuf );
        pQProc->selBuf = 0;
        }

    CloseQPOutputDC( pQProc, result );
    CloseQPInputFile( pQProc );

    if (!result)
        SplPanic( "SplQpRawPrintFile failed for %0s", pszFileName, 0 );

    return( result );
}
