/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * This file incorporates work covered by the following license notice:
 *
 *   Licensed to the Apache Software Foundation (ASF) under one or more
 *   contributor license agreements. See the NOTICE file distributed
 *   with this work for additional information regarding copyright
 *   ownership. The ASF licenses this file to you under the Apache
 *   License, Version 2.0 (the "License"); you may not use this file
 *   except in compliance with the License. You may obtain a copy of
 *   the License at http://www.apache.org/licenses/LICENSE-2.0 .
 */

#include <hintids.hxx>
#include <editeng/charscaleitem.hxx>
#include <txtatr.hxx>
#include <sfx2/printer.hxx>
#include <svx/svdobj.hxx>
#include <vcl/window.hxx>
#include <vcl/svapp.hxx>
#include <fmtanchr.hxx>
#include <fmtfsize.hxx>
#include <fmtornt.hxx>
#include <fmtflcnt.hxx>
#include <fmtcntnt.hxx>
#include <fmtftn.hxx>
#include <frmatr.hxx>
#include <frmfmt.hxx>
#include <fmtfld.hxx>
#include <doc.hxx>
#include <IDocumentSettingAccess.hxx>
#include <IDocumentDeviceAccess.hxx>
#include <viewsh.hxx>
#include <rootfrm.hxx>
#include <docary.hxx>
#include <ndtxt.hxx>
#include <dcontact.hxx>
#include <fldbas.hxx>
#include <pam.hxx>
#include <itratr.hxx>
#include <htmltbl.hxx>
#include <swtable.hxx>
#include <redlnitr.hxx>
#include <fmtsrnd.hxx>
#include <itrtxt.hxx>
#include <breakit.hxx>
#include <com/sun/star/i18n/WordType.hpp>
#include <com/sun/star/i18n/ScriptType.hpp>
#include <editeng/lrspitem.hxx>
#include <switerator.hxx>
#include <boost/foreach.hpp>

using namespace ::com::sun::star::i18n;
using namespace ::com::sun::star;

void SwAttrIter::Chg( SwTxtAttr *pHt )
{
    assert(pHt && pFnt && "No attribute of font available for change");
    if( pRedln && pRedln->IsOn() )
        pRedln->ChangeTxtAttr( pFnt, *pHt, true );
    else
        aAttrHandler.PushAndChg( *pHt, *pFnt );
    nChgCnt++;
}

void SwAttrIter::Rst( SwTxtAttr *pHt )
{
    assert(pHt && pFnt && "No attribute of font available for reset");
    // get top from stack after removing pHt
    if( pRedln && pRedln->IsOn() )
        pRedln->ChangeTxtAttr( pFnt, *pHt, false );
    else
        aAttrHandler.PopAndChg( *pHt, *pFnt );
    nChgCnt--;
}

SwAttrIter::~SwAttrIter()
{
    delete pRedln;
    delete pFnt;
}

// Liefert fuer eine Position das Attribut, wenn das Attribut genau auf
// der Position nPos liegt und kein EndIndex besitzt.
// GetAttr() wird fuer Attribute benoetigt, die die Formatierung beeinflussen
// sollen, ohne dabei den Inhalt des Strings zu veraendern. Solche "entarteten"
// Attribute sind z.B. Felder (die expandierten Text bereit halten) und
// zeilengebundene Frames. Um Mehrdeutigkeiten zwischen verschiedenen
// solcher Attribute zu vermeiden, werden beim Anlegen eines Attributs
// an der Startposition ein Sonderzeichen in den String einfuegt.
// Der Formatierer stoesst auf das Sonderzeichen und holt sich per
// GetAttr() das entartete Attribut.
SwTxtAttr *SwAttrIter::GetAttr( const sal_Int32 nPosition ) const
{
    return (m_pTxtNode) ? m_pTxtNode->GetTxtAttrForCharAt(nPosition) : 0;
}

bool SwAttrIter::SeekAndChgAttrIter( const sal_Int32 nNewPos, OutputDevice* pOut )
{
    bool bChg = nStartIndex && nNewPos == nPos ? pFnt->IsFntChg() : Seek( nNewPos );
    if ( pLastOut != pOut )
    {
        pLastOut = pOut;
        pFnt->SetFntChg( true );
        bChg = true;
    }
    if( bChg )
    {
        // if the change counter is zero, we know the MagicNo of the wanted font
        if ( !nChgCnt && !nPropFont )
            pFnt->SetMagic( aMagicNo[ pFnt->GetActual() ],
                aFntIdx[ pFnt->GetActual() ], pFnt->GetActual() );
        pFnt->ChgPhysFnt( pShell, *pOut );
    }

    return bChg;
}

bool SwAttrIter::IsSymbol( const sal_Int32 nNewPos )
{
    Seek( nNewPos );
    if ( !nChgCnt && !nPropFont )
        pFnt->SetMagic( aMagicNo[ pFnt->GetActual() ],
            aFntIdx[ pFnt->GetActual() ], pFnt->GetActual() );
    return pFnt->IsSymbol( pShell );
}

bool SwAttrIter::SeekStartAndChgAttrIter( OutputDevice* pOut, const bool bParaFont )
{
    if ( pRedln && pRedln->ExtOn() )
        pRedln->LeaveExtend( *pFnt, 0 );

    // reset font to its original state
    aAttrHandler.Reset();
    aAttrHandler.ResetFont( *pFnt );

    nStartIndex = 0;
    nEndIndex = 0;
    nPos = nChgCnt = 0;
    if( nPropFont )
        pFnt->SetProportion( nPropFont );
    if( pRedln )
    {
        pRedln->Clear( pFnt );
        if( !bParaFont )
            nChgCnt = nChgCnt + pRedln->Seek( *pFnt, 0, USHRT_MAX );
        else
            pRedln->Reset();
    }

    if ( pHints && !bParaFont )
    {
        SwTxtAttr *pTxtAttr;
        // While we've not reached the end of the StartArray && the TextAttribute starts at position 0...
        while ( ( nStartIndex < pHints->GetStartCount() ) &&
                !((pTxtAttr=pHints->GetStart(nStartIndex))->GetStart()) )
        {
            // open the TextAttributes
            Chg( pTxtAttr );
            nStartIndex++;
        }
    }

    bool bChg = pFnt->IsFntChg();
    if ( pLastOut != pOut )
    {
        pLastOut = pOut;
        pFnt->SetFntChg( true );
        bChg = true;
    }
    if( bChg )
    {
        // if the application counter is zero, we know the MagicNo of the wanted font
        if ( !nChgCnt && !nPropFont )
            pFnt->SetMagic( aMagicNo[ pFnt->GetActual() ],
                aFntIdx[ pFnt->GetActual() ], pFnt->GetActual() );
        pFnt->ChgPhysFnt( pShell, *pOut );
    }
    return bChg;
}

// AMA: New AttrIter Nov 94
void SwAttrIter::SeekFwd( const sal_Int32 nNewPos )
{
    SwTxtAttr *pTxtAttr;

    if ( nStartIndex ) // wenn ueberhaupt schon Attribute geoeffnet wurden...
    {
        // Schliesse Attr, die z. Z. geoeffnet sind, vor nNewPos+1 aber enden.

        // Solange wir noch nicht am Ende des EndArrays angekommen sind &&
        // das TextAttribut vor oder an der neuen Position endet ...
        while ( ( nEndIndex < pHints->GetEndCount() ) &&
                (*(pTxtAttr=pHints->GetEnd(nEndIndex))->GetAnyEnd()<=nNewPos))
        {
            // schliesse die TextAttribute, deren StartPos vor
            // oder an der alten nPos lag, die z.Z. geoeffnet sind.
            if (pTxtAttr->GetStart() <= nPos)  Rst( pTxtAttr );
            nEndIndex++;
        }
    }
    else // skip the not opended ends
    {
        while ( (nEndIndex < pHints->GetEndCount()) &&
                (*pHints->GetEnd(nEndIndex)->GetAnyEnd() <= nNewPos) )
        {
            nEndIndex++;
        }
    }
    // Solange wir noch nicht am Ende des StartArrays angekommen sind &&
    // das TextAttribut vor oder an der neuen Position beginnt ...
    while ( ( nStartIndex < pHints->GetStartCount() ) &&
            ((pTxtAttr=pHints->GetStart(nStartIndex))->GetStart()<=nNewPos) )
    {

        // open the TextAttributes, whose ends lie behind the new position
        if ( *pTxtAttr->GetAnyEnd() > nNewPos )  Chg( pTxtAttr );
        nStartIndex++;
    }

}

bool SwAttrIter::Seek( const sal_Int32 nNewPos )
{
    if ( pRedln && pRedln->ExtOn() )
        pRedln->LeaveExtend( *pFnt, nNewPos );

    if( pHints )
    {
        if( !nNewPos || nNewPos < nPos )
        {
            if( pRedln )
                pRedln->Clear( NULL );

            // reset font to its original state
            aAttrHandler.Reset();
            aAttrHandler.ResetFont( *pFnt );

            if( nPropFont )
                pFnt->SetProportion( nPropFont );
            nStartIndex = 0;
            nEndIndex = 0;
            nPos = 0;
            nChgCnt = 0;

            // Attention!
            // resetting the font here makes it necessary to apply any
            // changes for extended input directly to the font
            if ( pRedln && pRedln->ExtOn() )
            {
                pRedln->UpdateExtFont( *pFnt );
                ++nChgCnt;
            }
        }
        SeekFwd( nNewPos );
    }

    pFnt->SetActual( SwScriptInfo::WhichFont( nNewPos, 0, pScriptInfo ) );

    if( pRedln )
        nChgCnt = nChgCnt + pRedln->Seek( *pFnt, nNewPos, nPos );
    nPos = nNewPos;

    if( nPropFont )
        pFnt->SetProportion( nPropFont );

    return pFnt->IsFntChg();
}

sal_Int32 SwAttrIter::GetNextAttr( ) const
{
    sal_Int32 nNext = COMPLETE_STRING;
    if( pHints )
    {
        // are there attribute starts left?
        for (size_t i = nStartIndex; i < pHints->GetStartCount(); ++i)
        {
            SwTxtAttr *const pAttr(pHints->GetStart(i));
            if (!pAttr->IsFormatIgnoreStart())
            {
                nNext = pAttr->GetStart();
                break;
            }
        }
        // are there attribute ends left?
        for (size_t i = nEndIndex; i < pHints->GetEndCount(); ++i)
        {
            SwTxtAttr *const pAttr(pHints->GetEnd(i));
            if (!pAttr->IsFormatIgnoreEnd())
            {
                sal_Int32 const nNextEnd = *pAttr->GetAnyEnd();
                nNext = std::min(nNext, nNextEnd); // pick nearest one
                break;
            }
        }
    }
    if (m_pTxtNode!=NULL) {
        // TODO maybe use hints like FieldHints for this instead of looking at the text...
        const sal_Int32 l = nNext<m_pTxtNode->Len() ? nNext : m_pTxtNode->Len();
        sal_Int32 p=nPos;
        while (p<l && m_pTxtNode->GetTxt()[p] != CH_TXT_ATR_FIELDSTART
                   && m_pTxtNode->GetTxt()[p] != CH_TXT_ATR_FIELDEND
                   && m_pTxtNode->GetTxt()[p] != CH_TXT_ATR_FORMELEMENT)
        {
            ++p;
        }
        if ((p<l && p>nPos) || nNext<=p)
        nNext=p;
        else
        nNext=p+1;
    }
    if( pRedln )
        return pRedln->GetNextRedln( nNext );
    return nNext;
}

class SwMinMaxArgs
{
public:
    OutputDevice* pOut;
    SwViewShell* pSh;
    sal_uLong &rMin;
    sal_uLong &rMax;
    sal_uLong &rAbsMin;
    long nRowWidth;
    long nWordWidth;
    long nWordAdd;
    sal_Int32 nNoLineBreak;
    SwMinMaxArgs( OutputDevice* pOutI, SwViewShell* pShI, sal_uLong& rMinI, sal_uLong &rMaxI, sal_uLong &rAbsI )
        : pOut( pOutI ), pSh( pShI ), rMin( rMinI ), rMax( rMaxI ), rAbsMin( rAbsI )
        { nRowWidth = nWordWidth = nWordAdd = 0; nNoLineBreak = COMPLETE_STRING; }
    void Minimum( long nNew ) const { if( (long)rMin < nNew ) rMin = nNew; }
    void NewWord() { nWordAdd = nWordWidth = 0; }
};

static bool lcl_MinMaxString( SwMinMaxArgs& rArg, SwFont* pFnt, const OUString &rTxt,
    sal_Int32 nIdx, sal_Int32 nEnd )
{
    bool bRet = false;
    while( nIdx < nEnd )
    {
        sal_Int32 nStop = nIdx;
        bool bClear = false;
        LanguageType eLang = pFnt->GetLanguage();
        if( g_pBreakIt->GetBreakIter().is() )
        {
            bClear = CH_BLANK == rTxt[ nStop ];
            Boundary aBndry( g_pBreakIt->GetBreakIter()->getWordBoundary( rTxt, nIdx,
                             g_pBreakIt->GetLocale( eLang ),
                             WordType::DICTIONARY_WORD, true ) );
            nStop = aBndry.endPos;
            if( nIdx <= aBndry.startPos && nIdx && nIdx-1 != rArg.nNoLineBreak )
                rArg.NewWord();
            if( nStop == nIdx )
                ++nStop;
            if( nStop > nEnd )
                nStop = nEnd;
        }
        else
        {
            while( nStop < nEnd && CH_BLANK != rTxt[ nStop ] )
                ++nStop;
            bClear = nStop == nIdx;
            if ( bClear )
            {
                rArg.NewWord();
                while( nStop < nEnd && CH_BLANK == rTxt[ nStop ] )
                    ++nStop;
            }
        }

        SwDrawTextInfo aDrawInf( rArg.pSh, *rArg.pOut, 0, rTxt, nIdx, nStop - nIdx );
        long nAktWidth = pFnt->_GetTxtSize( aDrawInf ).Width();
        rArg.nRowWidth += nAktWidth;
        if( bClear )
            rArg.NewWord();
        else
        {
            rArg.nWordWidth += nAktWidth;
            if( (long)rArg.rAbsMin < rArg.nWordWidth )
                rArg.rAbsMin = rArg.nWordWidth;
            rArg.Minimum( rArg.nWordWidth + rArg.nWordAdd );
            bRet = true;
        }
        nIdx = nStop;
    }
    return bRet;
}

bool SwTxtNode::IsSymbol( const sal_Int32 nBegin ) const
{
    SwScriptInfo aScriptInfo;
    SwAttrIter aIter( *(SwTxtNode*)this, aScriptInfo );
    aIter.Seek( nBegin );
    return aIter.GetFnt()->IsSymbol(
        const_cast<SwViewShell *>(getIDocumentLayoutAccess()->GetCurrentViewShell()) );
}

class SwMinMaxNodeArgs
{
public:
    sal_uLong nMaxWidth;    // sum of all frame widths
    long nMinWidth;         // biggest frame
    long nLeftRest;         // space not already covered by frames in the left margin
    long nRightRest;        // space not already covered by frames in the right margin
    long nLeftDiff;         // Min/Max-difference of the frame in the left margin
    long nRightDiff;        // Min/Max-difference of the frame in the right margin
    sal_uLong nIndx;        // index of the node
    void Minimum( long nNew ) { if( nNew > nMinWidth ) nMinWidth = nNew; }
};

static void lcl_MinMaxNode( SwFrmFmt* pNd, SwMinMaxNodeArgs* pIn )
{
    const SwFmtAnchor& rFmtA = pNd->GetAnchor();

    bool bCalculate = false;
    if ((FLY_AT_PARA == rFmtA.GetAnchorId()) ||
        (FLY_AT_CHAR == rFmtA.GetAnchorId()))
    {
        bCalculate = true;
    }

    if (bCalculate)
    {
        const SwPosition *pPos = rFmtA.GetCntntAnchor();
        OSL_ENSURE(pPos && pIn, "Unexpected NULL arguments");
        if (!pPos || !pIn || pIn->nIndx != pPos->nNode.GetIndex())
            bCalculate = false;
    }

    if (bCalculate)
    {
        long nMin, nMax;
        SwHTMLTableLayout *pLayout = 0;
        sal_uInt16 nWhich = pNd->Which();
        if( RES_DRAWFRMFMT != nWhich )
        {
            // Does the frame contain a table at the start or the end?
            const SwNodes& rNodes = pNd->GetDoc()->GetNodes();
            const SwFmtCntnt& rFlyCntnt = pNd->GetCntnt();
            sal_uLong nStt = rFlyCntnt.GetCntntIdx()->GetIndex();
            SwTableNode* pTblNd = rNodes[nStt+1]->GetTableNode();
            if( !pTblNd )
            {
                SwNode *pNd2 = rNodes[nStt];
                pNd2 = rNodes[pNd2->EndOfSectionIndex()-1];
                if( pNd2->IsEndNode() )
                    pTblNd = pNd2->StartOfSectionNode()->GetTableNode();
            }

            if( pTblNd )
                pLayout = pTblNd->GetTable().GetHTMLTableLayout();
        }

        const SwFmtHoriOrient& rOrient = pNd->GetHoriOrient();
        sal_Int16 eHoriOri = rOrient.GetHoriOrient();

        long nDiff;
        if( pLayout )
        {
            nMin = pLayout->GetMin();
            nMax = pLayout->GetMax();
            nDiff = nMax - nMin;
        }
        else
        {
            if( RES_DRAWFRMFMT == nWhich )
            {
                const SdrObject* pSObj = pNd->FindSdrObject();
                if( pSObj )
                    nMin = pSObj->GetCurrentBoundRect().GetWidth();
                else
                nMin = 0;

            }
            else
            {
                const SwFmtFrmSize &rSz = pNd->GetFrmSize();
                nMin = rSz.GetWidth();
            }
            nMax = nMin;
            nDiff = 0;
        }

        const SvxLRSpaceItem &rLR = pNd->GetLRSpace();
        nMin += rLR.GetLeft();
        nMin += rLR.GetRight();
        nMax += rLR.GetLeft();
        nMax += rLR.GetRight();

        if( SURROUND_THROUGHT == pNd->GetSurround().GetSurround() )
        {
            pIn->Minimum( nMin );
            return;
        }

        // Frames, which are left- or right-aligned are only party considered
        // when calculating the maximum, since the border is already being considered.
        // Only if the frame extends into the text body, this part is being added
        switch( eHoriOri )
        {
            case text::HoriOrientation::RIGHT:
            {
                if( nDiff )
                {
                    pIn->nRightRest -= pIn->nRightDiff;
                    pIn->nRightDiff = nDiff;
                }
                if( text::RelOrientation::FRAME != rOrient.GetRelationOrient() )
                {
                    if( pIn->nRightRest > 0 )
                        pIn->nRightRest = 0;
                }
                pIn->nRightRest -= nMin;
                break;
            }
            case text::HoriOrientation::LEFT:
            {
                if( nDiff )
                {
                    pIn->nLeftRest -= pIn->nLeftDiff;
                    pIn->nLeftDiff = nDiff;
                }
                if( text::RelOrientation::FRAME != rOrient.GetRelationOrient() &&
                    pIn->nLeftRest < 0 )
                    pIn->nLeftRest = 0;
                pIn->nLeftRest -= nMin;
                break;
            }
            default:
            {
                pIn->nMaxWidth += nMax;
                pIn->Minimum( nMin );
            }
        }
    }
}

#define FLYINCNT_MIN_WIDTH 284

// changing this method very likely requires changing of
// "GetScalingOfSelectedText"
void SwTxtNode::GetMinMaxSize( sal_uLong nIndex, sal_uLong& rMin, sal_uLong &rMax,
                               sal_uLong& rAbsMin, OutputDevice* pOut ) const
{
    SwViewShell* pSh = 0;
    GetDoc()->GetEditShell( &pSh );
    if( !pOut )
    {
        if( pSh )
            pOut = pSh->GetWin();
        if( !pOut )
            pOut = Application::GetDefaultDevice();
    }

    MapMode aOldMap( pOut->GetMapMode() );
    pOut->SetMapMode( MapMode( MAP_TWIP ) );

    rMin = 0;
    rMax = 0;
    rAbsMin = 0;

    const SvxLRSpaceItem &rSpace = GetSwAttrSet().GetLRSpace();
    long nLROffset = rSpace.GetTxtLeft() + GetLeftMarginWithNum( true );
    short nFLOffs;
    // For enumerations a negative first line indentation is probably filled already
    if( !GetFirstLineOfsWithNum( nFLOffs ) || nFLOffs > nLROffset )
        nLROffset = nFLOffs;

    SwMinMaxNodeArgs aNodeArgs;
    aNodeArgs.nMinWidth = 0;
    aNodeArgs.nMaxWidth = 0;
    aNodeArgs.nLeftRest = nLROffset;
    aNodeArgs.nRightRest = rSpace.GetRight();
    aNodeArgs.nLeftDiff = 0;
    aNodeArgs.nRightDiff = 0;
    if( nIndex )
    {
        SwFrmFmts* pTmp = (SwFrmFmts*)GetDoc()->GetSpzFrmFmts();
        if( pTmp )
        {
            aNodeArgs.nIndx = nIndex;
            BOOST_FOREACH( SwFrmFmt *pFmt, *pTmp )
                lcl_MinMaxNode( pFmt, &aNodeArgs );
        }
    }
    if( aNodeArgs.nLeftRest < 0 )
        aNodeArgs.Minimum( nLROffset - aNodeArgs.nLeftRest );
    aNodeArgs.nLeftRest -= aNodeArgs.nLeftDiff;
    if( aNodeArgs.nLeftRest < 0 )
        aNodeArgs.nMaxWidth -= aNodeArgs.nLeftRest;

    if( aNodeArgs.nRightRest < 0 )
        aNodeArgs.Minimum( rSpace.GetRight() - aNodeArgs.nRightRest );
    aNodeArgs.nRightRest -= aNodeArgs.nRightDiff;
    if( aNodeArgs.nRightRest < 0 )
        aNodeArgs.nMaxWidth -= aNodeArgs.nRightRest;

    SwScriptInfo aScriptInfo;
    SwAttrIter aIter( *(SwTxtNode*)this, aScriptInfo );
    sal_Int32 nIdx = 0;
    aIter.SeekAndChgAttrIter( nIdx, pOut );
    sal_Int32 nLen = m_Text.getLength();
    long nAktWidth = 0;
    sal_uInt16 nAdd = 0;
    SwMinMaxArgs aArg( pOut, pSh, rMin, rMax, rAbsMin );
    while( nIdx < nLen )
    {
        sal_Int32 nNextChg = aIter.GetNextAttr();
        sal_Int32 nStop = aScriptInfo.NextScriptChg( nIdx );
        if( nNextChg > nStop )
            nNextChg = nStop;
        SwTxtAttr *pHint = NULL;
        sal_Unicode cChar = CH_BLANK;
        nStop = nIdx;
        while( nStop < nLen && nStop < nNextChg &&
               CH_TAB != ( cChar = m_Text[nStop] ) &&
               CH_BREAK != cChar && CHAR_HARDBLANK != cChar &&
               CHAR_HARDHYPHEN != cChar && CHAR_SOFTHYPHEN != cChar &&
               !pHint )
        {
            if( ( CH_TXTATR_BREAKWORD != cChar && CH_TXTATR_INWORD != cChar )
                || ( 0 == ( pHint = aIter.GetAttr( nStop ) ) ) )
                ++nStop;
        }
        if ( lcl_MinMaxString( aArg, aIter.GetFnt(), m_Text, nIdx, nStop ) )
        {
            nAdd = 20;
        }
        nIdx = nStop;
        aIter.SeekAndChgAttrIter( nIdx, pOut );
        switch( cChar )
        {
            case CH_BREAK  :
            {
                if( (long)rMax < aArg.nRowWidth )
                    rMax = aArg.nRowWidth;
                aArg.nRowWidth = 0;
                aArg.NewWord();
                aIter.SeekAndChgAttrIter( ++nIdx, pOut );
            }
            break;
            case CH_TAB    :
            {
                aArg.NewWord();
                aIter.SeekAndChgAttrIter( ++nIdx, pOut );
            }
            break;
            case CHAR_SOFTHYPHEN:
                ++nIdx;
            break;
            case CHAR_HARDBLANK:
            case CHAR_HARDHYPHEN:
            {
                OUString sTmp( cChar );
                SwDrawTextInfo aDrawInf( const_cast<SwViewShell *>(getIDocumentLayoutAccess()->GetCurrentViewShell()),
                    *pOut, 0, sTmp, 0, 1, 0, false );
                nAktWidth = aIter.GetFnt()->_GetTxtSize( aDrawInf ).Width();
                aArg.nWordWidth += nAktWidth;
                aArg.nRowWidth += nAktWidth;
                if( (long)rAbsMin < aArg.nWordWidth )
                    rAbsMin = aArg.nWordWidth;
                aArg.Minimum( aArg.nWordWidth + aArg.nWordAdd );
                aArg.nNoLineBreak = nIdx++;
            }
            break;
            case CH_TXTATR_BREAKWORD:
            case CH_TXTATR_INWORD:
            {
                if( !pHint )
                    break;
                long nOldWidth = aArg.nWordWidth;
                long nOldAdd = aArg.nWordAdd;
                aArg.NewWord();

                switch( pHint->Which() )
                {
                    case RES_TXTATR_FLYCNT :
                    {
                        SwFrmFmt *pFrmFmt = pHint->GetFlyCnt().GetFrmFmt();
                        const SvxLRSpaceItem &rLR = pFrmFmt->GetLRSpace();
                        if( RES_DRAWFRMFMT == pFrmFmt->Which() )
                        {
                            const SdrObject* pSObj = pFrmFmt->FindSdrObject();
                            if( pSObj )
                                nAktWidth = pSObj->GetCurrentBoundRect().GetWidth();
                            else
                                nAktWidth = 0;
                        }
                        else
                        {
                            const SwFmtFrmSize& rTmpSize = pFrmFmt->GetFrmSize();
                            if( RES_FLYFRMFMT == pFrmFmt->Which()
                                && rTmpSize.GetWidthPercent() )
                            {
                                // This is a hack for the wollowing situation: In the paragraph there's a
                                // text frame with relative size. Then let's take 0.5 cm as minimum width
                                // and USHRT_MAX as maximum width
                                // It were cleaner and maybe necessary later on to iterate over the content
                                // of the text frame and call GetMinMaxSize recursively
                                nAktWidth = FLYINCNT_MIN_WIDTH; // 0.5 cm
                                if( (long)rMax < USHRT_MAX )
                                    rMax = USHRT_MAX;
                            }
                            else
                                nAktWidth = pFrmFmt->GetFrmSize().GetWidth();
                        }
                        nAktWidth += rLR.GetLeft();
                        nAktWidth += rLR.GetRight();
                        aArg.nWordAdd = nOldWidth + nOldAdd;
                        aArg.nWordWidth = nAktWidth;
                        aArg.nRowWidth += nAktWidth;
                        if( (long)rAbsMin < aArg.nWordWidth )
                            rAbsMin = aArg.nWordWidth;
                        aArg.Minimum( aArg.nWordWidth + aArg.nWordAdd );
                        break;
                    }
                    case RES_TXTATR_FTN :
                    {
                        const OUString aTxt = pHint->GetFtn().GetNumStr();
                        if( lcl_MinMaxString( aArg, aIter.GetFnt(), aTxt, 0,
                            aTxt.getLength() ) )
                            nAdd = 20;
                        break;
                    }

                    case RES_TXTATR_FIELD :
                    case RES_TXTATR_ANNOTATION :
                        {
                            SwField *pFld = (SwField*)pHint->GetFmtFld().GetField();
                            const OUString aTxt = pFld->ExpandField(true);
                            if( lcl_MinMaxString( aArg, aIter.GetFnt(), aTxt, 0,
                                aTxt.getLength() ) )
                                nAdd = 20;
                            break;
                        }
                    default: aArg.nWordWidth = nOldWidth;
                        aArg.nWordAdd = nOldAdd;

                }
                aIter.SeekAndChgAttrIter( ++nIdx, pOut );
            }
            break;
        }
    }
    if( (long)rMax < aArg.nRowWidth )
        rMax = aArg.nRowWidth;

    nLROffset += rSpace.GetRight();

    rAbsMin += nLROffset;
    rAbsMin += nAdd;
    rMin += nLROffset;
    rMin += nAdd;
    if( (long)rMin < aNodeArgs.nMinWidth )
        rMin = aNodeArgs.nMinWidth;
    if( (long)rAbsMin < aNodeArgs.nMinWidth )
        rAbsMin = aNodeArgs.nMinWidth;
    rMax += aNodeArgs.nMaxWidth;
    rMax += nLROffset;
    rMax += nAdd;
    if( rMax < rMin ) // z.B. Rahmen mit Durchlauf gehen zunaechst nur
        rMax = rMin;  // in das Minimum ein
    pOut->SetMapMode( aOldMap );
}

// Calculates the width of the text part specified by nStt and nEnd,
// the height of the line containing nStt is devided by this width,
// indicating the scaling factor, if the text part is rotated.
// Having CH_BREAKs in the text part, this method returns the scaling
// factor for the longest of the text parts separated by the CH_BREAKs.
//
// changing this method very likely requires changing of "GetMinMaxSize"
sal_uInt16 SwTxtNode::GetScalingOfSelectedText( sal_Int32 nStt, sal_Int32 nEnd )
    const
{
    SwViewShell* pSh = NULL;
    OutputDevice* pOut = NULL;
    GetDoc()->GetEditShell( &pSh );

    if ( pSh )
        pOut = &pSh->GetRefDev();
    else
    {
        // Access via StarONE, there's no need for an existing or active shell
        if ( getIDocumentSettingAccess()->get(IDocumentSettingAccess::HTML_MODE) )
            pOut = Application::GetDefaultDevice();
        else
            pOut = getIDocumentDeviceAccess()->getReferenceDevice( true );
    }

    OSL_ENSURE( pOut, "GetScalingOfSelectedText without outdev" );

    MapMode aOldMap( pOut->GetMapMode() );
    pOut->SetMapMode( MapMode( MAP_TWIP ) );

    if ( nStt == nEnd )
    {
        if ( !g_pBreakIt->GetBreakIter().is() )
            return 100;

        SwScriptInfo aScriptInfo;
        SwAttrIter aIter( *(SwTxtNode*)this, aScriptInfo );
        aIter.SeekAndChgAttrIter( nStt, pOut );

        Boundary aBound =
            g_pBreakIt->GetBreakIter()->getWordBoundary( GetTxt(), nStt,
            g_pBreakIt->GetLocale( aIter.GetFnt()->GetLanguage() ),
            WordType::DICTIONARY_WORD, true );

        if ( nStt == aBound.startPos )
        {
            // cursor is at left or right border of word
            pOut->SetMapMode( aOldMap );
            return 100;
        }

        nStt = aBound.startPos;
        nEnd = aBound.endPos;

        if ( nStt == nEnd )
        {
            pOut->SetMapMode( aOldMap );
            return 100;
        }
    }

    SwScriptInfo aScriptInfo;
    SwAttrIter aIter( *(SwTxtNode*)this, aScriptInfo );

    // We do not want scaling attributes to be considered during this
    // calculation. For this, we push a temporary scaling attribute with
    // scaling value 100 and priority flag on top of the scaling stack
    SwAttrHandler& rAH = aIter.GetAttrHandler();
    SvxCharScaleWidthItem aItem(100, RES_CHRATR_SCALEW);
    SwTxtAttrEnd aAttr( aItem, nStt, nEnd );
    aAttr.SetPriorityAttr( true );
    rAH.PushAndChg( aAttr, *(aIter.GetFnt()) );

    sal_Int32 nIdx = nStt;

    sal_uLong nWidth = 0;
    sal_uLong nProWidth = 0;

    while( nIdx < nEnd )
    {
        aIter.SeekAndChgAttrIter( nIdx, pOut );

        // scan for end of portion
        const sal_Int32 nNextChg = std::max(aIter.GetNextAttr(), aScriptInfo.NextScriptChg( nIdx ));

        sal_Int32 nStop = nIdx;
        sal_Unicode cChar = CH_BLANK;
        SwTxtAttr* pHint = NULL;

        // stop at special characters in [ nIdx, nNextChg ]
        while( nStop < nEnd && nStop < nNextChg )
        {
            cChar = m_Text[nStop];
            if (
                CH_TAB == cChar ||
                CH_BREAK == cChar ||
                CHAR_HARDBLANK == cChar ||
                CHAR_HARDHYPHEN == cChar ||
                CHAR_SOFTHYPHEN == cChar ||
                (
                  (CH_TXTATR_BREAKWORD == cChar || CH_TXTATR_INWORD == cChar) &&
                  (0 == (pHint = aIter.GetAttr(nStop)))
                )
               )
            {
                break;
            }
            else
                ++nStop;
        }

        // calculate text widths up to cChar
        if ( nStop > nIdx )
        {
            SwDrawTextInfo aDrawInf( pSh, *pOut, 0, GetTxt(), nIdx, nStop - nIdx );
            nProWidth += aIter.GetFnt()->_GetTxtSize( aDrawInf ).Width();
        }

        nIdx = nStop;
        aIter.SeekAndChgAttrIter( nIdx, pOut );

        if ( cChar == CH_BREAK )
        {
            nWidth = std::max( nWidth, nProWidth );
            nProWidth = 0;
            nIdx++;
        }
        else if ( cChar == CH_TAB )
        {
            // tab receives width of one space
            OUString sTmp( CH_BLANK );
            SwDrawTextInfo aDrawInf( pSh, *pOut, 0, sTmp, 0, 1 );
            nProWidth += aIter.GetFnt()->_GetTxtSize( aDrawInf ).Width();
            nIdx++;
        }
        else if ( cChar == CHAR_SOFTHYPHEN )
            ++nIdx;
        else if ( cChar == CHAR_HARDBLANK || cChar == CHAR_HARDHYPHEN )
        {
            OUString sTmp( cChar );
            SwDrawTextInfo aDrawInf( pSh, *pOut, 0, sTmp, 0, 1 );
            nProWidth += aIter.GetFnt()->_GetTxtSize( aDrawInf ).Width();
            nIdx++;
        }
        else if ( pHint && ( cChar == CH_TXTATR_BREAKWORD || cChar == CH_TXTATR_INWORD ) )
        {
            switch( pHint->Which() )
            {
            case RES_TXTATR_FTN :
                {
                    const OUString aTxt = pHint->GetFtn().GetNumStr();
                    SwDrawTextInfo aDrawInf( pSh, *pOut, 0, aTxt, 0, aTxt.getLength() );

                    nProWidth += aIter.GetFnt()->_GetTxtSize( aDrawInf ).Width();
                    break;
                }

            case RES_TXTATR_FIELD :
            case RES_TXTATR_ANNOTATION :
                {
                    SwField *pFld = (SwField*)pHint->GetFmtFld().GetField();
                    OUString const aTxt = pFld->ExpandField(true);
                    SwDrawTextInfo aDrawInf( pSh, *pOut, 0, aTxt, 0, aTxt.getLength() );

                    nProWidth += aIter.GetFnt()->_GetTxtSize( aDrawInf ).Width();
                    break;
                }

            default:
                {
                    // any suggestions for a default action?
                }
            } // end of switch
            nIdx++;
        } // end of while
    }

    nWidth = std::max( nWidth, nProWidth );

    // search for a text frame this node belongs to
    SwIterator<SwTxtFrm,SwTxtNode> aFrmIter( *this );
    SwTxtFrm* pFrm = 0;
    for( SwTxtFrm* pTmpFrm = aFrmIter.First(); pTmpFrm; pTmpFrm = aFrmIter.Next() )
    {
            if ( pTmpFrm->GetOfst() <= nStt &&
                ( !pTmpFrm->GetFollow() ||
                   pTmpFrm->GetFollow()->GetOfst() > nStt ) )
            {
                pFrm = pTmpFrm;
                break;
            }
        }

    // search for the line containing nStt
    if ( pFrm && pFrm->HasPara() )
    {
        SwTxtInfo aInf( pFrm );
        SwTxtIter aLine( pFrm, &aInf );
        aLine.CharToLine( nStt );
        pOut->SetMapMode( aOldMap );
        return (sal_uInt16)( nWidth ?
            ( ( 100 * aLine.GetCurr()->Height() ) / nWidth ) : 0 );
    }
    // no frame or no paragraph, we take the height of the character
    // at nStt as line height

    aIter.SeekAndChgAttrIter( nStt, pOut );
    pOut->SetMapMode( aOldMap );

    SwDrawTextInfo aDrawInf( pSh, *pOut, 0, GetTxt(), nStt, 1 );
    return (sal_uInt16)
           ( nWidth ? ((100 * aIter.GetFnt()->_GetTxtSize( aDrawInf ).Height()) / nWidth ) : 0 );
}

SwTwips SwTxtNode::GetWidthOfLeadingTabs() const
{
    SwTwips nRet = 0;

    sal_Int32 nIdx = 0;

    while ( nIdx < GetTxt().getLength() )
    {
        const sal_Unicode cCh = GetTxt()[nIdx];
        if ( cCh!='\t' && cCh!=' ' )
        {
            break;
        }
        ++nIdx;
    }

    if ( nIdx > 0 )
    {
        SwPosition aPos( *this );
        aPos.nContent += nIdx;

        // Find the non-follow text frame:
        SwIterator<SwTxtFrm,SwTxtNode> aIter( *this );
        for( SwTxtFrm* pFrm = aIter.First(); pFrm; pFrm = aIter.Next() )
        {
            // Only consider master frames:
            if ( !pFrm->IsFollow() )
            {
                SWRECTFN( pFrm )
                SwRect aRect;
                pFrm->GetCharRect( aRect, aPos );
                nRet = pFrm->IsRightToLeft() ?
                            (pFrm->*fnRect->fnGetPrtRight)() - (aRect.*fnRect->fnGetRight)() :
                            (aRect.*fnRect->fnGetLeft)() - (pFrm->*fnRect->fnGetPrtLeft)();
                break;
            }
        }
    }

    return nRet;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
