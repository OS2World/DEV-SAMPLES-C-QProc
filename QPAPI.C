/*****************************************************************/ 
/**		     Microsoft LAN Manager			**/ 
/**	       Copyright(c) Microsoft Corp., 1985-1990		**/ 
/*****************************************************************/ 
/****************************** Module Header ******************************\

Module Name: QPAPI.C

This module contains the default PM Print Queue Processor dynlink entry
points.

ENTRY POINTS:   SplQpOpen, SplQpPrint, SplQpClose, SplQpControl,
                SplQpQueryDt, SplQpInstall

History:
 21-Aug-88 [stevewo]  Created.
 14-Feb-90 [thomaspa] DBCS fixes.

\***************************************************************************/

#define INCL_DEV
#define INCL_GPIMETAFILES
#define INCL_GPICONTROL
#define INCL_DOSFILEMGR
#define INCL_SHLERRORS
#define INCL_WINDIALOGS
#define INCL_GPIP
#define INCL_DEVP
#include "pmprint.h"
#include <netlib.h>
#include <strapi.h>
#include <makenear.h>



/***************************** Public  Function ****************************\
*
*
* SplQpQueryDt - return list of data types supported by this queue processor
*
*
\***************************************************************************/

BOOL SPLENTRY SplQpQueryDt( pcDataType, rgpszDataType )
PLONG pcDataType;
PSZ FAR *rgpszDataType;

/* Description: ************************************************************\

History:
 21-Aug-88 [stevewo]  Created
\***************************************************************************/
{
    USHORT c;

    if (!pcDataType) {
        SplLogError(PMERR_INVALID_PARM);
    } else {
        if (*pcDataType > QP_TYPE_NUM)
           *pcDataType = QP_TYPE_NUM;

        c = (USHORT)*pcDataType;
        *pcDataType = QP_TYPE_NUM;

        if (!c)
            return( TRUE );

        if (c != (USHORT)*pcDataType) {
            SplLogError(PMERR_SPL_INV_LENGTH_OR_COUNT);
        } else if (!rgpszDataType || !rgpszDataType[0] || !rgpszDataType[1]) {
            SplLogError(PMERR_INVALID_PARM);
        } else {
            SafeStrcpy( rgpszDataType[QP_TYPE_STD], DT_STD );
            SafeStrcpy( rgpszDataType[QP_TYPE_RAW], DT_RAW );

            return( TRUE );
        }
    }
    return( FALSE );
}


/***************************** Public  Function ****************************\
*
*
* SplQpOpen - Open Queue Process to begin printing
*
*
\***************************************************************************/

HPROC SPLENTRY SplQpOpen( cData, pQPDataIn )
LONG cData;
PQPOPENDATA pQPDataIn;

/* Description: ************************************************************\


History:
 21-Aug-88 [stevewo]  Created
\***************************************************************************/
{
    register PQPROCINST pQProc;
    DEVOPENSTRUC OpenData;
    PSQPOPENDATA pSQPData = (PSQPOPENDATA) pQPDataIn;

    pQProc = NULL;

    if (cData < QPDAT_PROC_PARAMS - 1) {
        SplLogError(PMERR_SPL_INV_LENGTH_OR_COUNT);
        return NULL;
    }

    OpenData.pszLogAddress = pQPDataIn[ QPDAT_ADDRESS ];
    OpenData.pszDriverName = pQPDataIn[ QPDAT_DRIVER_NAME ];
    OpenData.pdriv         = (PDRIVDATA)pQPDataIn[ QPDAT_DRIVER_DATA ];
    OpenData.pszComment    = pQPDataIn[ QPDAT_COMMENT ];

    if (!(OpenData.pszDataType = pQPDataIn[ QPDAT_DATA_TYPE ] ))
        OpenData.pszDataType = DT_STD;

    if (!(OpenData.pszQueueProcParams = pQPDataIn[ QPDAT_PROC_PARAMS ]))
        OpenData.pszQueueProcParams = szNull;

    pQProc = CreateQProcInst(&OpenData, ((PSQPOPENDATA)pQPDataIn)->idJobId,
                             ((PSQPOPENDATA)pQPDataIn)->pszQueueName);

    return( pQProc ? (HPROC)(USHORT)pQProc : (HPROC)NULL );
}


/***************************** Public  Function ****************************\
*
*
* SplQpPrint - print a file
*
*
\***************************************************************************/

BOOL SPLENTRY SplQpPrint( hQProc, pszFileName )
HPROC hQProc;
PSZ pszFileName;

/* Description: ************************************************************\


History:
 21-Aug-88 [stevewo]  Created
\***************************************************************************/
{
    register PQPROCINST pQProc;
    BOOL result;

  EnterSplSem();
    pQProc = ValidateQProcInst( hQProc );
  LeaveSplSem();

    result = FALSE;
    DosError( FALSE );

    if (pQProc && pszFileName)
        while (pQProc->qparms.cCopies &&
              (result = (BOOL)(*pQProc->pfnPrintFile)( pQProc, pszFileName )))
            pQProc->qparms.cCopies--;

    DosError( TRUE );

    return( result );
}



/***************************** Public  Function ****************************\
*
*
* SplQpClose - close a queue processor
*
*
\***************************************************************************/

BOOL SPLENTRY SplQpClose( hQProc )
HPROC hQProc;

/* Description: ************************************************************\


History:
 21-Aug-88 [stevewo]  Created
\***************************************************************************/
{
    register PQPROCINST pQProc;
    BOOL result = TRUE;

  EnterSplSem();

    if (pQProc = ValidateQProcInst( hQProc ))
        DestroyQProcInst( pQProc );
    else
        result = FALSE;

  LeaveSplSem();

    return( result );
}


/***************************** Public  Function ****************************\
*
*
* SplQpControl - abort, pause or continue the current printing operation
*
*
\***************************************************************************/

BOOL SPLENTRY SplQpControl( hQProc, ulControlCode )
HPROC hQProc;
LONG ulControlCode;

/* Description: ************************************************************\


History:
 21-Aug-88 [stevewo]  Created
\***************************************************************************/
{
    register PQPROCINST pQProc;
    BOOL result;
    USHORT  usStopMetaFile=SDW_OFF;

  EnterSplSem();
    if (pQProc = ValidateQProcInst( hQProc )) {
        result = TRUE;
        switch( (USHORT)ulControlCode ) {
            case SPLC_PAUSE:
                if (pQProc->uType == QP_TYPE_STD)
                   while(!GpiSuspendPlay( pQProc->hps ))
                       ; /* The Hursley spooler does this - so we will */

                DosSemSet(&pQProc->semPaused);
                pQProc->fsStatus |= QP_PAUSED;
                break;

            case SPLC_ABORT:
                pQProc->fsStatus |= QP_ABORTED;

                if (pQProc->uType == QP_TYPE_STD) {
                    if (pQProc->hps) 
                        GpiSetStopDraw( pQProc->hps, SDW_ON );
                }

                if (pQProc->hdc) {
                   DevEscape(pQProc->hdc, DEVESC_ABORTDOC, 0L, NULL, 0L, NULL);
                }

                /* fall through to release job is paused */

            case SPLC_CONTINUE:
                if (pQProc->fsStatus & QP_PAUSED) {

                    if (pQProc->uType == QP_TYPE_STD)
                        GpiResumePlay( pQProc->hps );

                    DosSemClear(&pQProc->semPaused);
                    pQProc->fsStatus &= ~QP_PAUSED;
                }
                break;

            default:
                SplLogError(PMERR_INVALID_PARM);
                result = FALSE;
                break;
            }
        }
    else
        result = FALSE;
  LeaveSplSem();

    return( result );
}


/***************************** Public  Function ****************************\
*
*
* SplQpInstall - display installation message
*
*
\***************************************************************************/

BOOL EXPENTRY _loadds SplQpInstall( hwnd )
HWND hwnd;
{
    register USHORT us;

    us = WinMessageBox(HWND_DESKTOP, hwnd, INSTALL_MSG, INSTALL_CAPTION,
                       0, MB_OK | MB_INFORMATION | MB_MOVEABLE);
    return us && us != MBID_ERROR;
}



PQPROCINST CreateQProcInst(PDEVOPENSTRUC pQProcData,
                           USHORT uJobId, PSZ pszQName)
{
    register PQPROCINST pQProc;
    PSZ psz;
    NPSZ pStr;
    PDRIVDATA   pDriverData;
    USHORT      cb;
    USHORT      cbDriverData;
    USHORT      cbDataType;
    USHORT      cbComment;
    USHORT      cbPortName;
    USHORT      cbDriverName;
    USHORT      cbQName;
    USHORT      uDataType;
    QPPARMS     qp_parms;
    PSZ         pszComment;


    SplOutSem();

    if (pDriverData = pQProcData->pdriv)
        cbDriverData = (USHORT)pDriverData->cb;
    else
        cbDriverData = 0;

    cbDataType = 1 + SafeStrlen( psz = pQProcData->pszDataType );
    if (!strcmpf( psz, DT_STD ))
        uDataType = QP_TYPE_STD;
    else
    if (!strcmpf( psz, DT_RAW ))
        uDataType = QP_TYPE_RAW;
    else {
        SplQpMessage(pQProcData->pszLogAddress, SPL_ID_QP_DATATYPE_ERROR,
                     PMERR_SPL_INV_DATATYPE);
        return( NULL );
    }
    cbComment = 1+SafeStrlen(pszComment = pQProcData->pszComment ?
                                       pQProcData->pszComment : (PSZ)szNull);
    cbDriverName = 1+SafeStrlen( pQProcData->pszDriverName );
    cbQName = 1+SafeStrlen(pszQName);
    cbPortName = 1+SafeStrlen( pQProcData->pszLogAddress );

    if (!ParseQProcParms( pQProcData->pszQueueProcParams, &qp_parms )) {
        if (SplQpMessage(pQProcData->pszLogAddress, SPL_ID_QP_INVALID_PARAMETER,
                         PMERR_INVALID_PARM) == MBID_CANCEL)
            return NULL;
    }

    cb = sizeof( QPROCINST ) + cbDataType + cbComment + cbQName +
                               cbDriverName + cbPortName + cbDriverData;

  EnterSplSem();
    if (!(pQProc = (PQPROCINST)AllocSplMem( cb ))) {
      LeaveSplSem();
        SplQpMessage(pQProcData->pszLogAddress, SPL_ID_QP_MEM_ERROR,
                     PMERR_SPL_NO_MEMORY);
        return( NULL );
    }
    memsetf(pQProc, 0, sizeof(QPROCINST));
    pStr = (NPSZ)(pQProc + 1);

    pQProc->pNext         = pQProcInstances;
    pQProcInstances       = pQProc;
    pQProc->cb            = cb;
    pQProc->signature     = QP_SIGNATURE;
    pQProc->uPid	  = pLocalInfo->pidCurrent;
    pQProc->uJobId	  = uJobId;

    DosSemClear(&pQProc->semPaused);

    pQProc->uType         = uDataType;
    pQProc->hFile         = NULL_HFILE;
    pQProc->pszDataType   = pStr;
    pQProc->pszDocument   = NULL;
    pStr = 1 + (NPSZ)MAKENEAR(EndStrcpy( pStr, pQProcData->pszDataType ));
    pQProc->pszQName      = pStr;
    pStr = 1 + (NPSZ)MAKENEAR(EndStrcpy( pStr, pszQName));
    pQProc->pszComment    = pStr;
    pStr = 1 + (NPSZ)MAKENEAR(EndStrcpy( pStr, pszComment));
    pQProc->pszDriverName = pStr;
    pStr = 1 + (NPSZ)MAKENEAR(EndStrcpy( pStr, pQProcData->pszDriverName ));
    pQProc->pszPortName   = pStr;
    pStr = 1 + (NPSZ)MAKENEAR(EndStrcpy( pStr, pQProcData->pszLogAddress ));

    if (cbDriverData) {
        pQProc->pDriverData = (PDRIVDATA)(PSZ)pStr;
        memcpyf( pQProc->pDriverData, pDriverData, cbDriverData );
    } else
        pQProc->pDriverData = (PDRIVDATA)0l;

    pQProc->qparms = qp_parms;

    switch( pQProc->uType ) {
        case QP_TYPE_STD: pQProc->pfnPrintFile = SplQpStdPrintFile; break;
        case QP_TYPE_RAW: pQProc->pfnPrintFile = SplQpRawPrintFile; break;
    }
  LeaveSplSem();

    return( pQProc );
}


PQPROCINST FARENTRY DestroyQProcInst( pQProc )
register PQPROCINST pQProc;
{
    register PQPROCINST pQProc1 = pQProcInstances;

    if (!pQProc || pQProc->signature != QP_SIGNATURE)
        return( NULL );

    if (pQProc == pQProc1)
        pQProcInstances = pQProc->pNext;
    else
        while (pQProc1)
            if (pQProc1->pNext == pQProc) {
                pQProc1->pNext = pQProc->pNext;
                break;
                }
            else
                pQProc1 = pQProc1->pNext;

    if (!pQProc1)
        SplPanic( "DestroyQProcInst pQProc=%0x not in list", pQProc, 0 );
    pQProc1 = pQProc->pNext;

    pQProc->signature = 0;

    /* Release any allocated resources */

    if (pQProc->hmf) GpiDeleteMetaFile( pQProc->hmf );
    if (pQProc->hps) GpiAssociate( pQProc->hps, (HDC)0 );
    if (pQProc->hdc) DevCloseDC( pQProc->hdc );
    if (pQProc->hps) GpiDestroyPS( pQProc->hps );
    if (pQProc->hFile != NULL_HFILE) DosClose( pQProc->hFile );

    FreeSplStr( pQProc->pszFileName );
    FreeSplStr( pQProc->pszDocument );

    if (pQProc->selBuf)
        DosFreeSeg( pQProc->selBuf );

    FreeSplMem( (NPBYTE)pQProc, pQProc->cb );

    return( pQProc1 );
}


PQPROCINST ValidateQProcInst( hQProc )
HPROC hQProc;
{
    register PQPROCINST pQProc = hQProcTopQProc( hQProc );

    if (pQProc && ChkMem(pQProc, sizeof(QPROCINST), CHK_WRITE) &&
        pQProc->signature == QP_SIGNATURE)
        return( pQProc );
    else {
        SplPanic( "Invalid hQProc parameter = %0p", hQProc, 0 );
        SplLogError(PMERR_SPL_INV_HSPL);
        return( NULL );
    }
}


/* ParseQProcParms -- parses queue processor parameter string
 *
 * in:  pszParms - -> parameter string
 *      pqp - -> buffer
 * out: string contains only valid parameters?
 *      *pqp - specified parameter
 */
BOOL ParseQProcParms( pszParms, pqp )
PSZ pszParms;
PQPPARMS pqp;
{
    register PKEYDATA pQPParms=0;
    register NPSZ pParm;
    USHORT i;
    BOOL result = TRUE;

    memsetf(pqp, 0, sizeof(QPPARMS));
    pqp->cCopies = 1;
    pqp->fTransform = TRUE;

    if (!pszParms || !(i = SafeStrlen( pszParms )))
        return( TRUE );

  EnterSplSem();
    if (pQPParms = ParseKeyData( pszParms, ' ', i+1 ))
        for (i=0; i<pQPParms->cTokens; i++)
            if (pParm = pQPParms->pTokens[ i ])
                result &= ParseQProcParm(pParm, pqp);

    if (pQPParms)
        FreeSplMem(pQPParms, pQPParms->cb);
  LeaveSplSem();

    if (!result)
        SplWarning( "Invalid queue processor parms: %0s", pszParms, 0 );

    return( result );
}


BOOL ParseQProcParm( pszParm, pqp )
register NPSZ pszParm;
PQPPARMS pqp;
{
    register NPSZ pszVal = pszParm+3;
    USHORT us;

    if (SafeStrlen( pszParm ) < 5)
        return( FALSE );

    if (*pszVal != '=')
        return( FALSE );
    *pszVal++ = '\0';

    if (!strcmpf( pszParm, "COP" )) {
        if (!(us = AsciiToInt( pszVal )))
            return( FALSE );
        else
            pqp->cCopies = us;

        while ((UCHAR)(*pszVal-'0') < 10)
            pszVal++;
        }
    else
    if (!strcmpf( pszParm, "CDP" )) {	  /* Code Page Support */
	pqp->uCodePage = AsciiToInt( pszVal );

        while ((UCHAR)(*pszVal-'0') < 10)
	    pszVal++;

	if(*pszVal != '\0') {
	    pqp->uCodePage = 0;
	    return( FALSE );
	    };
        }
    else
    if (!strcmpf( pszParm, "XFM" )) {
        if (*pszVal == '0')
            pqp->fTransform = FALSE;
        else
        if (*pszVal == '1')
            pqp->fTransform = TRUE;
        else
            return( FALSE );

        pszVal++;
        }
    else
    if (!strcmpf( pszParm, "ORI" )) {
        if (*pszVal == 'P')
            pqp->fLandscape = FALSE;
        else
        if (*pszVal == 'L')
            pqp->fLandscape = TRUE;
        else
            return( FALSE );

        pszVal++;
        }
    else
    if (!strcmpf( pszParm, "COL" )) {
        if (*pszVal == 'M')
            pqp->fColor = FALSE;
        else
        if (*pszVal == 'C')
            pqp->fColor = TRUE;
        else
            return( FALSE );

        pszVal++;
        }
    else
    if (!strcmpf( pszParm, "MAP" )) {
        if (*pszVal == 'N')
            pqp->fMapColors = FALSE;
        else
        if (*pszVal == 'A')
            pqp->fMapColors = TRUE;
        else
            return( FALSE );

        pszVal++;
        }
    else
    if (!strcmpf( pszParm, "ARE" )) {
        if (*pszVal == 'C') {
            pqp->fArea = FALSE;
            pszVal++;
            }
        else
        if (pszVal = ParseQProcPercentList( pszVal, (PBYTE)&pqp->ptAreaSize, 4 ))
            pqp->fArea = TRUE;
        else
            return( FALSE );
        }
    else
    if (!strcmpf( pszParm, "FIT" )) {
        if (*pszVal == 'S') {
            pqp->fFit = FALSE;
            pszVal++;
            }
        else
        if (pszVal = ParseQProcPercentList( pszVal, (PBYTE)&pqp->ptFit, 2 ))
            pqp->fFit = TRUE;
        else
            return( FALSE );
        }
    else
        return( FALSE );

    return( *pszVal == '\0' );
}


NPSZ ParseQProcPercentList( pszList, pResult, cListElem )
register NPSZ pszList;
PBYTE pResult;
USHORT cListElem;
{
    register NPSZ p = pszList;
    USHORT n;

    while (*pszList && cListElem)
        if (!*p || *p == ',') {
            if (!cListElem--)
                return( NULL );

            if (*p == ',')
                *p++ = '\0';

            if ((n = AsciiToInt( pszList )) > 100)
                return( NULL );

            *pResult++ = (BYTE)n;
            pszList = p;
            }
        else
            p++;

    if (cListElem)
        return( NULL );
    else
        return( p );
}


BOOL OpenQPInputFile(register PQPROCINST pQProc, PSZ pszFileName, BOOL fOpen)
{
    USHORT actionTaken,rc;
    FILESTATUS inFileStatus;
    USHORT  NoFileHandles=30;

    SplOutSem();
    if (pszFileName) {
      EnterSplSem();
        FreeSplStr( pQProc->pszFileName );
        pQProc->pszFileName = AllocSplStr(pszFileName);
      LeaveSplSem();
        }

    if (pQProc->pszFileName) {
        if (pQProc->hFile == NULL_HFILE)
            while (((rc=DosOpen( (PSZ)pQProc->pszFileName,
                            (PHFILE)&pQProc->hFile,
                            (PUSHORT)&actionTaken,
                            (ULONG)0, (USHORT)0,
                            OS_OF_EXIST,         /* open only if exists */
                            OM_PRIVATE | OM_SHARED | OM_READ,
                            (ULONG)0 )) == ERROR_TOO_MANY_OPEN_FILES) &&
                ((rc=DosSetMaxFH(NoFileHandles++)) != ERROR_NOT_ENOUGH_MEMORY))
                ;
        else
            rc=TRUE;
        if (rc) {
            SplPanic("QP: DosOpen failed for %0s", (PSZ)pQProc->pszFileName, 0);
            SplQpMessage(pQProc->pszPortName, SPL_ID_QP_FILE_NOT_FOUND,
                         PMERR_SPL_FILE_NOT_FOUND);
        } else if (pQProc->hFile != NULL_HFILE) {
            if (!DosQFileInfo( pQProc->hFile, 1,
                              (PBYTE)&inFileStatus,
                              sizeof( inFileStatus ) ) )
                pQProc->cbFile = inFileStatus.cbFile;

            if (!fOpen) {
                DosClose( pQProc->hFile );
                pQProc->hFile = NULL_HFILE;
            }
            return( TRUE );
        }
    }
    return( FALSE );
}


BOOL CloseQPInputFile( pQProc )
register PQPROCINST pQProc;
{
    if (pQProc->hFile != NULL_HFILE) {
        DosClose( pQProc->hFile );
        pQProc->hFile = NULL_HFILE;
    }

    return( TRUE );
}



BOOL OpenQPOutputDC(register PQPROCINST pQProc, USHORT fFlag)
{
    DEVOPENSTRUC openData;
    NPSZ pszDocument;
    ESCSETMODE ModeData;
#ifdef LMPRINT
    DEVICESPL dev;
#endif

    SplOutSem();

    openData.pszLogAddress = (PSZ)pQProc->pszPortName;
    openData.pszDriverName = (PSZ)pQProc->pszDriverName;
    openData.pdriv         = pQProc->pDriverData;
    openData.pszDataType   = pQProc->pszDataType;
    openData.pszComment    = pQProc->pszComment;

    pQProc->hdc = DevOpenDC( HABX, OD_DIRECT, "*",
                             (LONG)DATA_TYPE+1,
                             (PDEVOPENDATA)&openData,
                             (HDC)NULL );

    if (!pQProc->hdc) {
        SplPanic( "QP: DevOpenDC failed for %0s on %4s",
                  openData.pszLogAddress, openData.pszDriverName );
        SplQpMessage(pQProc->pszPortName, SPL_ID_QP_OPENDC_ERROR, 0);
    } else {

      if (fFlag == ASSOCIATE) {
          GpiAssociate (pQProc->hps, pQProc->hdc); 
      }

#ifdef LMPRINT
        if (GetSepInfo(pQProc, &dev) == OKSIG) {
            if (DevEscape(pQProc->hdc, DEVESC_STARTDOC, 0L, NULL,
                          NULL, NULL) != DEVESC_ERROR) {
                if(!strcmpf(pQProc->pszDataType, DT_RAW)) {
                    ModeData.mode = 0;
                    ModeData.codepage = usCodePage;

                    DevEscape(pQProc->hdc, DEVESC_SETMODE,
                              (ULONG)sizeof(ModeData),
                              (PBYTE)&ModeData,
                              0L,
                              NULL);
                }
                PrintSep(&dev);
                DevEscape(pQProc->hdc, DEVESC_ENDDOC, 0L, NULL, NULL, NULL);
            }
        }
#endif /* LMPRINT */

        if (DevEscape(pQProc->hdc, DEVESC_STARTDOC,
                      (pszDocument = pQProc->pszDocument) ?
                                    (ULONG)(SafeStrlen(pszDocument) + 1) : 0L,
		      pszDocument, NULL, NULL) != DEVESC_ERROR) {
	    /* Code Page support */
	    if((!strcmpf(pQProc->pszDataType, DT_RAW))&&(pQProc->qparms.uCodePage))
	    {
                ModeData.mode = 0;
                ModeData.codepage = pQProc->qparms.uCodePage;

		DevEscape(pQProc->hdc, DEVESC_SETMODE,
                          (ULONG)sizeof(ModeData),
			  (char far *)&ModeData,
			  0L,
			  NULL);
	    };

	    return( TRUE );
	};
    }

    return( FALSE );
}


BOOL CloseQPOutputDC(register PQPROCINST pQProc, BOOL fEndDoc)
{
    BOOL result = TRUE;

    SplOutSem();
    if (pQProc->hdc) {
        if (fEndDoc &&
            !(pQProc->fsStatus & QP_ABORTED) &&
            !DevEscape( pQProc->hdc, DEVESC_ENDDOC, 0L,
                        (PBYTE)NULL, (PLONG)NULL, (PBYTE)NULL
                      ) != DEVESC_ERROR)
            result = FALSE;

        DevCloseDC( pQProc->hdc );
        pQProc->hdc = NULL;
    }

    return( result );
}
