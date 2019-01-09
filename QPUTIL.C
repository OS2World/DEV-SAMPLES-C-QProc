/*****************************************************************/ 
/**		     Microsoft LAN Manager			**/ 
/**	       Copyright(c) Microsoft Corp., 1985-1990		**/ 
/*****************************************************************/ 
/****************************** Module Header ******************************\

Module Name: QPUTIL.C

Utility functions for PM Print Queue Processor

History:
 10-Sep-88 [stevewo]  Created.
 14-Feb-90 [thomaspa] DBCS fixes.

\***************************************************************************/

#define INCL_WINATOM
#include "pmprint.h"
#include <netlib.h>
#include <strapi.h>
#include <makenear.h>

#define NEXTCHAR( psz )		if( IS_LEAD_BYTE( *psz ) )	\
				{				\
					psz += 2;		\
				}				\
				else				\
				{				\
					psz++;			\
				}
/*
 * MakeNearPtr
 * Runtime test for a pointer outside the default data segment
 *
 * In non-debugging versions, this is replaced by a simple macro
 * that returns the offset-portion of the pointer.
 *
 */
#if defined(DEBUG)
void near * pascal far
MakeNearPtr( void far * fp )
{
    /* HIWORD and LOWORD both generate better code
       and work on rvalues */
    if(SELECTOROF(fp) != HIUSHORT(fp))
        SplWarning( "MakeNearPtr: SELECTOROF(fp) != HIUSHORT(fp)", 0, 0);
    if(OFFSETOF(fp) != LOUSHORT(fp))
	SplWarning( "MakeNearPtr: OFFSETOF(fp) != LOUSHORT(fp)", 0, 0);
    /* assumption: SS == DS */
    if(fp != NULL && HIUSHORT(fp) != HIUSHORT((char far *)&fp))
	SplWarning( "MakeNearPtr: HIUSHORT(fp) != HIUSHORT((char far *)&fp)", 0,0);
    return LOUSHORT(fp);
}
#endif // DEBUG



VOID FARENTRY EnterSplSem( VOID )
{
    if (!bInitDone)
        SplPanic( "Spooler not initialized", 0, 0 );
    else
        FSRSemEnter( &semPMPRINT );
}

VOID FARENTRY LeaveSplSem( VOID )
{
    FSRSemLeave( &semPMPRINT );
}

VOID FARENTRY ExitSplSem( VOID )
{
    FSRSemExit( &semPMPRINT );
}


/* AllocSplMem -- allocates memory in default DS
 *
 * in:  cb - size of memory requested
 * out: -> new memory, NULL if error
 */
NPVOID FARENTRY AllocSplMem(USHORT cb)
{
    NPVOID p;

    p = WinAllocMem( hSplHeap, cb );
#ifdef LATER
    SplWarning( "AllocSplMem( %0d ) = %2x", cb, p );
#endif
    if (p) {
#ifdef DEBUG
        memsetf(p, 0xbc, cb);  /* put in signature */
#endif
        SplInSem();
        return( p );
    } else {
        SplErrNoMemory( cb );
    }
}


/* AllocSplStr -- Allocates mem for a string and copies it into
 *
 * in:  pszSrc - -> new string, may be NULL or null string
 * out: new string, NULL if source was empty
 */
NPSZ FARENTRY AllocSplStr(PSZ pszSrc)
{
    NPSZ p = NULL;
    USHORT cbSrc;
                                        /* change only if src != NULL */
    if (pszSrc && (cbSrc = SafeStrlen(pszSrc))) {
        if (p = AllocSplMem(cbSrc + 1))
            strcpyf(p, pszSrc);
    }
    return p;
}


/* FreeSplMem -- frees memory allocated by AllocSplMem
 *
 * in:  p - -> memory allocated by AllocSplMem or NULL
 *      cb - size specified on AllocSplMem
 * out: -> memory if memory couldn't freed, otherwise NULL
 */
NPVOID FARENTRY FreeSplMem( p, cb )
NPVOID p;
USHORT cb;
{
#ifdef DEBUG
    if (p) {
        SplInSem();
        memsetf(p, 0xab, cb);  /* put in signature */
    }
#endif
    if (p) {
#ifdef LATER
        SplWarning( "FreeSplMem( %0x, %2d )", p, cb );
#endif
        if (p = WinFreeMem( hSplHeap, p, cb ))
            SplPanic( "SplFreeMem - invalid pointer = %0x", p, 0 );
    }

    return( p );
}


/* FreeSplStr -- frees memory allocated by AllocSplStr
 *
 * in:  psz - -> memory allocated by AllocSplMem or NULL
 * out: -> memory if memory couldn't freed, otherwise NULL
 */
NPSZ FARENTRY FreeSplStr(NPSZ psz)
{
    return psz ? FreeSplMem(psz, SafeStrlen(psz) + 1) : NULL;
}


USHORT FARENTRY AsciiToInt( psz )
PSZ psz;
{
    USHORT n;
    UCHAR c;
    BOOL bNegative = FALSE;
    BOOL bDigitSeen= FALSE;

    while (*psz == ' ')
        psz++;

    c = *psz;
    if (c == '-')
        bNegative = TRUE;
    else
    if (c != '+')
        c = 0;

    if (c)
        psz++;

    n = 0;
    while (c = *psz++) {
        c -= '0';
        if (c > 9)
            break;

        else {
            bDigitSeen = TRUE;
            n = (n*10) + c;
            }
        }

    if (bNegative)
        return( -n );
    else
        return( n );
}




/*
 * ParseKeyData -- parses string for a separator character and
 *                 allocates and builds an array of strings out of it,
 *                 each string is zero-terminated, followed by an extra \0
 * in:  pKeyData - -> string to parse
 *      chSep - separator character
 *      cbKeyData - length of the string
 * out: -> string array allocated on near heap, must be freed by caller
 */
PKEYDATA FARENTRY ParseKeyData( pKeyData, chSep, cbKeyData )
PSZ pKeyData;
UCHAR chSep;
USHORT cbKeyData;
{
    USHORT cTokens, cb, i;
    register PKEYDATA pResult;
    register NPSZ pDst;
    PSZ psz;

    cTokens = 0;
    cb = cbKeyData;
    if (psz = pKeyData) {
        while (cb-- && *psz)
        {
            if (*psz == chSep)
                cTokens++;
            NEXTCHAR(psz);  
        }

        if (psz[-1] != chSep)
            if (chSep != ';')
                cTokens++;
            else
                cTokens = 0;
        }

    if (cb || *psz || !cTokens)
        return( NULL );

    cb = sizeof( KEYDATA ) + (cTokens-1) * sizeof( NPSZ ) + cbKeyData;
    if (!(pResult = (PKEYDATA)AllocSplMem( cb )))
        return( NULL );

    pResult->cb = cb;
    pResult->cTokens = cTokens;
    pDst = (NPSZ)pResult + (cb-cbKeyData);
    i = 0;
    while (cTokens--) {
        pResult->pTokens[ i ] = pDst;
        while (*pDst = *pKeyData++) {
            if (*pDst != chSep)
	    {
                NEXTCHAR(pDst);
            }
            else {
                *pDst = '\0';
                break;
                }
            }

        if (pResult->pTokens[ i ] == pDst)
            pResult->pTokens[ i ] = NULL;
        else
        {
            NEXTCHAR(pDst);
        }
        i++;
    }

    return( pResult );
}


PSZ FARENTRY MyItoa(USHORT wNumber, PSZ pszRes)
{
    PSZ  pch,pchEnd;
    CHAR  ch;

    pch = pszRes;                     /* pch points on 1rst CHAR */
    do {
        *pszRes++ = (CHAR)(wNumber % 10 + '0');
    } while ((wNumber /= 10) > 0);

    pchEnd=pszRes;
    *pszRes-- = '\0';                  /* pszRes points on last CHAR */

    while (pch < pszRes) {
        ch = *pch;
        *pch++ = *pszRes;
        *pszRes-- = ch;
    }
    return (pchEnd);      /* return pointer to terminating 0 byte */
}
