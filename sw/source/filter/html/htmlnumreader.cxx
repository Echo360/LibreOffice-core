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

#include <com/sun/star/style/NumberingType.hpp>
#include <hintids.hxx>
#include <svtools/htmltokn.h>
#include <svtools/htmlkywd.hxx>
#include <svtools/htmlout.hxx>
#include <svl/urihelper.hxx>
#include <editeng/brushitem.hxx>
#include <editeng/lrspitem.hxx>
#include <vcl/svapp.hxx>
#include <vcl/wrkwin.hxx>
#include <numrule.hxx>
#include <doc.hxx>
#include <docary.hxx>
#include <poolfmt.hxx>
#include <ndtxt.hxx>
#include <paratr.hxx>

#include "htmlnum.hxx"
#include "swcss1.hxx"
#include "swhtml.hxx"

#include <SwNodeNum.hxx>

using namespace css;

// <UL TYPE=...>
static HTMLOptionEnum aHTMLULTypeTable[] =
{
    { OOO_STRING_SVTOOLS_HTML_ULTYPE_disc,      HTML_BULLETCHAR_DISC   },
    { OOO_STRING_SVTOOLS_HTML_ULTYPE_circle,    HTML_BULLETCHAR_CIRCLE },
    { OOO_STRING_SVTOOLS_HTML_ULTYPE_square,    HTML_BULLETCHAR_SQUARE },
    { 0,                                        0                      }
};


void SwHTMLParser::NewNumBulList( int nToken )
{
    SwHTMLNumRuleInfo& rInfo = GetNumInfo();

    // Erstmal einen neuen Absatz aufmachen
    bool bSpace = (rInfo.GetDepth() + nDefListDeep) == 0;
    if( pPam->GetPoint()->nContent.GetIndex() )
        AppendTxtNode( bSpace ? AM_SPACE : AM_NOSPACE, false );
    else if( bSpace )
        AddParSpace();

    // Die Numerierung-Ebene erhoehen
    rInfo.IncDepth();
    sal_uInt8 nLevel = (sal_uInt8)( (rInfo.GetDepth() <= MAXLEVEL ? rInfo.GetDepth()
                                                        : MAXLEVEL) - 1 );

    // ggf. ein Regelwerk anlegen
    if( !rInfo.GetNumRule() )
    {
        sal_uInt16 nPos = pDoc->MakeNumRule( pDoc->GetUniqueNumRuleName() );
        rInfo.SetNumRule( pDoc->GetNumRuleTbl()[nPos] );
    }

    // das Format anpassen, falls es fuer den Level noch nicht
    // geschehen ist!
    bool bNewNumFmt = rInfo.GetNumRule()->GetNumFmt( nLevel ) == 0;
    bool bChangeNumFmt = false;

    // das default Numerierungsformat erstellen
    SwNumFmt aNumFmt( rInfo.GetNumRule()->Get(nLevel) );
    rInfo.SetNodeStartValue( nLevel );
    if( bNewNumFmt )
    {
        sal_uInt16 nChrFmtPoolId = 0;
        if( HTML_ORDERLIST_ON == nToken )
        {
            aNumFmt.SetNumberingType(SVX_NUM_ARABIC);
            nChrFmtPoolId = RES_POOLCHR_NUM_LEVEL;
        }
        else
        {
            // Wir setzen hier eine Zeichenvorlage, weil die UI das auch
            // so macht. Dadurch wurd immer auch eine 9pt-Schrift
            // eingestellt, was in Netscape nicht der Fall ist. Bisher hat
            // das noch niemanden gestoert.
            // #i63395# - Only apply user defined default bullet font
            if ( numfunc::IsDefBulletFontUserDefined() )
            {
                aNumFmt.SetBulletFont( &numfunc::GetDefBulletFont() );
            }
            aNumFmt.SetNumberingType(SVX_NUM_CHAR_SPECIAL);
            aNumFmt.SetBulletChar( cBulletChar );       // das Bulletzeichen !!
            nChrFmtPoolId = RES_POOLCHR_BUL_LEVEL;
        }

        sal_uInt16 nAbsLSpace = HTML_NUMBUL_MARGINLEFT;

        short nFirstLineIndent  = HTML_NUMBUL_INDENT;
        if( nLevel > 0 )
        {
            const SwNumFmt& rPrevNumFmt = rInfo.GetNumRule()->Get( nLevel-1 );
            nAbsLSpace = nAbsLSpace + rPrevNumFmt.GetAbsLSpace();
            nFirstLineIndent = rPrevNumFmt.GetFirstLineOffset();
        }
        aNumFmt.SetAbsLSpace( nAbsLSpace );
        aNumFmt.SetFirstLineOffset( nFirstLineIndent );
        aNumFmt.SetCharFmt( pCSS1Parser->GetCharFmtFromPool(nChrFmtPoolId) );

        bChangeNumFmt = true;
    }
    else if( 1 != aNumFmt.GetStart() )
    {
        // Wenn die Ebene schon mal benutzt wurde, muss der Start-Wert
        // ggf. hart am Absatz gesetzt werden.
        rInfo.SetNodeStartValue( nLevel, 1 );
    }

    // und es ggf. durch die Optionen veraendern
    OUString aId, aStyle, aClass, aLang, aDir;
    OUString aBulletSrc;
    sal_Int16 eVertOri = text::VertOrientation::NONE;
    sal_uInt16 nWidth=USHRT_MAX, nHeight=USHRT_MAX;
    const HTMLOptions& rHTMLOptions = GetOptions();
    for (size_t i = rHTMLOptions.size(); i; )
    {
        const HTMLOption& rOption = rHTMLOptions[--i];
        switch( rOption.GetToken() )
        {
        case HTML_O_ID:
            aId = rOption.GetString();
            break;
        case HTML_O_TYPE:
            if( bNewNumFmt && !rOption.GetString().isEmpty() )
            {
                switch( nToken )
                {
                case HTML_ORDERLIST_ON:
                    bChangeNumFmt = true;
                    switch( rOption.GetString()[0] )
                    {
                    case 'A':   aNumFmt.SetNumberingType(SVX_NUM_CHARS_UPPER_LETTER); break;
                    case 'a':   aNumFmt.SetNumberingType(SVX_NUM_CHARS_LOWER_LETTER); break;
                    case 'I':   aNumFmt.SetNumberingType(SVX_NUM_ROMAN_UPPER);        break;
                    case 'i':   aNumFmt.SetNumberingType(SVX_NUM_ROMAN_LOWER);        break;
                    default:    bChangeNumFmt = false;
                    }
                    break;

                case HTML_UNORDERLIST_ON:
                    aNumFmt.SetBulletChar( (sal_Unicode)rOption.GetEnum(
                                    aHTMLULTypeTable,aNumFmt.GetBulletChar() ) );
                    bChangeNumFmt = true;
                    break;
                }
            }
            break;
        case HTML_O_START:
            {
                sal_uInt16 nStart = (sal_uInt16)rOption.GetNumber();
                if( bNewNumFmt )
                {
                    aNumFmt.SetStart( nStart );
                    bChangeNumFmt = true;
                }
                else
                {
                    rInfo.SetNodeStartValue( nLevel, nStart );
                }
            }
            break;
        case HTML_O_STYLE:
            aStyle = rOption.GetString();
            break;
        case HTML_O_CLASS:
            aClass = rOption.GetString();
            break;
        case HTML_O_LANG:
            aLang = rOption.GetString();
            break;
        case HTML_O_DIR:
            aDir = rOption.GetString();
            break;
        case HTML_O_SRC:
            if( bNewNumFmt )
            {
                aBulletSrc = rOption.GetString();
                if( !InternalImgToPrivateURL(aBulletSrc) )
                    aBulletSrc = URIHelper::SmartRel2Abs( INetURLObject( sBaseURL ), aBulletSrc, Link(), false );
            }
            break;
        case HTML_O_WIDTH:
            nWidth = (sal_uInt16)rOption.GetNumber();
            break;
        case HTML_O_HEIGHT:
            nHeight = (sal_uInt16)rOption.GetNumber();
            break;
        case HTML_O_ALIGN:
            eVertOri =
                (sal_Int16)rOption.GetEnum( aHTMLImgVAlignTable,
                                                static_cast< sal_uInt16 >(eVertOri) );
            break;
        }
    }

    if( !aBulletSrc.isEmpty() )
    {
        // Eine Bullet-Liste mit Grafiken
        aNumFmt.SetNumberingType(SVX_NUM_BITMAP);

        // Die Grafik als Brush anlegen
        SvxBrushItem aBrushItem( RES_BACKGROUND );
        aBrushItem.SetGraphicLink( aBulletSrc );
        aBrushItem.SetGraphicPos( GPOS_AREA );

        // Die Groesse nur beachten, wenn Breite und Hoehe vorhanden sind
        Size aTwipSz( nWidth, nHeight), *pTwipSz=0;
        if( nWidth!=USHRT_MAX && nHeight!=USHRT_MAX )
        {
            aTwipSz =
                Application::GetDefaultDevice()->PixelToLogic( aTwipSz,
                                                    MapMode(MAP_TWIP) );
            pTwipSz = &aTwipSz;
        }

        // Die Ausrichtung auch nur beachten, wenn eine Ausrichtung
        // angegeben wurde
        aNumFmt.SetGraphicBrush( &aBrushItem, pTwipSz,
                            text::VertOrientation::NONE!=eVertOri ? &eVertOri : 0);

        // Und noch die Grafik merken, um sie in den Absaetzen nicht
        // einzufuegen
        aBulletGrfs[nLevel] = aBulletSrc;
        bChangeNumFmt = true;
    }
    else
        aBulletGrfs[nLevel] = "";

    // den aktuellen Absatz erst einmal nicht numerieren
    {
        sal_uInt8 nLvl = nLevel;
        SetNodeNum( nLvl, false );
    }

    // einen neuen Kontext anlegen
    _HTMLAttrContext *pCntxt = new _HTMLAttrContext( static_cast< sal_uInt16 >(nToken) );

    // Styles parsen
    if( HasStyleOptions( aStyle, aId, aClass, &aLang, &aDir ) )
    {
        SfxItemSet aItemSet( pDoc->GetAttrPool(), pCSS1Parser->GetWhichMap() );
        SvxCSS1PropertyInfo aPropInfo;

        if( ParseStyleOptions( aStyle, aId, aClass, aItemSet, aPropInfo, &aLang, &aDir ) )
        {
            if( bNewNumFmt )
            {
                if( aPropInfo.bLeftMargin )
                {
                    // Der Der Default-Einzug wurde schon eingefuegt.
                    sal_uInt16 nAbsLSpace =
                        aNumFmt.GetAbsLSpace() - HTML_NUMBUL_MARGINLEFT;
                    if( aPropInfo.nLeftMargin < 0 &&
                        nAbsLSpace < -aPropInfo.nLeftMargin )
                        nAbsLSpace = 0U;
                    else if( aPropInfo.nLeftMargin > USHRT_MAX ||
                             (long)nAbsLSpace +
                                            aPropInfo.nLeftMargin > USHRT_MAX )
                        nAbsLSpace = USHRT_MAX;
                    else
                        nAbsLSpace = nAbsLSpace + (sal_uInt16)aPropInfo.nLeftMargin;

                    aNumFmt.SetAbsLSpace( nAbsLSpace );
                    bChangeNumFmt = true;
                }
                if( aPropInfo.bTextIndent )
                {
                    short nTextIndent =
                        ((const SvxLRSpaceItem &)aItemSet.Get( RES_LR_SPACE ))
                                                        .GetTxtFirstLineOfst();
                    aNumFmt.SetFirstLineOffset( nTextIndent );
                    bChangeNumFmt = true;
                }
            }
            aPropInfo.bLeftMargin = aPropInfo.bTextIndent = false;
            if( !aPropInfo.bRightMargin )
                aItemSet.ClearItem( RES_LR_SPACE );

            // #i89812# - Perform change to list style before calling <DoPositioning(..)>,
            // because <DoPositioning(..)> may open a new context and thus may
            // clear the <SwHTMLNumRuleInfo> instance hold by local variable <rInfo>.
            if( bChangeNumFmt )
            {
                rInfo.GetNumRule()->Set( nLevel, aNumFmt );
                pDoc->ChgNumRuleFmts( *rInfo.GetNumRule() );
                bChangeNumFmt = false;
            }

            DoPositioning( aItemSet, aPropInfo, pCntxt );

            InsertAttrs( aItemSet, aPropInfo, pCntxt );
        }
    }

    if( bChangeNumFmt )
    {
        rInfo.GetNumRule()->Set( nLevel, aNumFmt );
        pDoc->ChgNumRuleFmts( *rInfo.GetNumRule() );
    }

    PushContext( pCntxt );

    // die Attribute der neuen Vorlage setzen
    SetTxtCollAttrs( pCntxt );
}

void SwHTMLParser::EndNumBulList( int nToken )
{
    SwHTMLNumRuleInfo& rInfo = GetNumInfo();

    // Ein neuer Absatz muss aufgemacht werden, wenn
    // - der aktuelle nicht leer ist, also Text oder absatzgebundene Objekte
    //   enthaelt.
    // - der aktuelle Absatz numeriert ist.
    bool bAppend = pPam->GetPoint()->nContent.GetIndex() > 0;
    if( !bAppend )
    {
        SwTxtNode* pTxtNode = pPam->GetNode().GetTxtNode();

        bAppend = (pTxtNode && ! pTxtNode->IsOutline() && pTxtNode->IsCountedInList()) ||

            HasCurrentParaFlys();
    }

    bool bSpace = (rInfo.GetDepth() + nDefListDeep) == 1;
    if( bAppend )
        AppendTxtNode( bSpace ? AM_SPACE : AM_NOSPACE, false );
    else if( bSpace )
        AddParSpace();

    // den aktuellen Kontext vom Stack holen
    _HTMLAttrContext *pCntxt = nToken!=0 ? PopContext( static_cast< sal_uInt16 >(nToken & ~1) ) : 0;

    // Keine Liste aufgrund eines Tokens beenden, wenn der Kontext
    // nie angelgt wurde oder nicht beendet werden darf.
    if( rInfo.GetDepth()>0 && (!nToken || pCntxt) )
    {
        rInfo.DecDepth();
        if( !rInfo.GetDepth() )     // wars der letze Level ?
        {
            // Die noch nicht angepassten Formate werden jetzt noch
            // angepasst, damit es sich besser Editieren laesst.
            const SwNumFmt *pRefNumFmt = 0;
            bool bChanged = false;
            for( sal_uInt16 i=0; i<MAXLEVEL; i++ )
            {
                const SwNumFmt *pNumFmt = rInfo.GetNumRule()->GetNumFmt(i);
                if( pNumFmt )
                {
                    pRefNumFmt = pNumFmt;
                }
                else if( pRefNumFmt )
                {
                    SwNumFmt aNumFmt( rInfo.GetNumRule()->Get(i) );
                    aNumFmt.SetNumberingType(pRefNumFmt->GetNumberingType() != SVX_NUM_BITMAP
                                        ? pRefNumFmt->GetNumberingType() : style::NumberingType::CHAR_SPECIAL);
                    if( SVX_NUM_CHAR_SPECIAL == aNumFmt.GetNumberingType() )
                    {
                        // #i63395# - Only apply user defined default bullet font
                        if ( numfunc::IsDefBulletFontUserDefined() )
                        {
                            aNumFmt.SetBulletFont( &numfunc::GetDefBulletFont() );
                        }
                        aNumFmt.SetBulletChar( cBulletChar );
                    }
                    aNumFmt.SetAbsLSpace( (i+1) * HTML_NUMBUL_MARGINLEFT );
                    aNumFmt.SetFirstLineOffset( HTML_NUMBUL_INDENT );
                    aNumFmt.SetCharFmt( pRefNumFmt->GetCharFmt() );
                    rInfo.GetNumRule()->Set( i, aNumFmt );
                    bChanged = true;
                }
            }
            if( bChanged )
                pDoc->ChgNumRuleFmts( *rInfo.GetNumRule() );

            // Beim letzen Append wurde das NumRule-Item und das
            // NodeNum-Objekt mit kopiert. Beides muessen wir noch
            // loeschen. Das ResetAttr loescht das NodeNum-Objekt mit!
            pPam->GetNode().GetTxtNode()->ResetAttr( RES_PARATR_NUMRULE );

            rInfo.Clear();
        }
        else
        {
            // the next paragraph not numbered first
            SetNodeNum( rInfo.GetLevel(), false );
        }
    }

    // und noch Attribute beenden
    bool bSetAttrs = false;
    if( pCntxt )
    {
        EndContext( pCntxt );
        delete pCntxt;
        bSetAttrs = true;
    }

    if( nToken )
        SetTxtCollAttrs();

    if( bSetAttrs )
        SetAttr();  // Absatz-Atts wegen JavaScript moeglichst schnell setzen

}

void SwHTMLParser::NewNumBulListItem( int nToken )
{
    sal_uInt8 nLevel = GetNumInfo().GetLevel();
    OUString aId, aStyle, aClass, aLang, aDir;
    sal_uInt16 nStart = HTML_LISTHEADER_ON != nToken
                        ? GetNumInfo().GetNodeStartValue( nLevel )
                        : USHRT_MAX;
    if( USHRT_MAX != nStart )
        GetNumInfo().SetNodeStartValue( nLevel );

    const HTMLOptions& rHTMLOptions = GetOptions();
    for (size_t i = rHTMLOptions.size(); i; )
    {
        const HTMLOption& rOption = rHTMLOptions[--i];
        switch( rOption.GetToken() )
        {
            case HTML_O_VALUE:
                nStart = (sal_uInt16)rOption.GetNumber();
                break;
            case HTML_O_ID:
                aId = rOption.GetString();
                break;
            case HTML_O_STYLE:
                aStyle = rOption.GetString();
                break;
            case HTML_O_CLASS:
                aClass = rOption.GetString();
                break;
            case HTML_O_LANG:
                aLang = rOption.GetString();
                break;
            case HTML_O_DIR:
                aDir = rOption.GetString();
                break;
        }
    }

    // einen neuen Absatz aufmachen
    if( pPam->GetPoint()->nContent.GetIndex() )
        AppendTxtNode( AM_NOSPACE, false );
    bNoParSpace = false;    // In <LI> wird kein Abstand eingefuegt!

    const bool bCountedInList( HTML_LISTHEADER_ON==nToken ? false : true );

    _HTMLAttrContext *pCntxt = new _HTMLAttrContext( static_cast< sal_uInt16 >(nToken) );

    OUString aNumRuleName;
    if( GetNumInfo().GetNumRule() )
    {
        aNumRuleName = GetNumInfo().GetNumRule()->GetName();
    }
    else
    {
        aNumRuleName = pDoc->GetUniqueNumRuleName();
        SwNumRule aNumRule( aNumRuleName,
                            SvxNumberFormat::LABEL_WIDTH_AND_POSITION );
        SwNumFmt aNumFmt( aNumRule.Get( 0 ) );
        // #i63395# - Only apply user defined default bullet font
        if ( numfunc::IsDefBulletFontUserDefined() )
        {
            aNumFmt.SetBulletFont( &numfunc::GetDefBulletFont() );
        }
        aNumFmt.SetNumberingType(SVX_NUM_CHAR_SPECIAL);
        aNumFmt.SetBulletChar( cBulletChar );   // das Bulletzeichen !!
        aNumFmt.SetCharFmt( pCSS1Parser->GetCharFmtFromPool(RES_POOLCHR_BUL_LEVEL) );
        aNumFmt.SetLSpace( (sal_uInt16)(-HTML_NUMBUL_INDENT) );
        aNumFmt.SetFirstLineOffset( HTML_NUMBUL_INDENT );
        aNumRule.Set( 0, aNumFmt );

        pDoc->MakeNumRule( aNumRuleName, &aNumRule );

        OSL_ENSURE( !nOpenParaToken,
                "Jetzt geht ein offenes Absatz-Element verloren" );
        // Wir tun so, als ob wir in einem Absatz sind. Dann wird
        // beim naechsten Absatz wenigstens die Numerierung
        // weggeschmissen, die nach dem naechsten AppendTxtNode uebernommen
        // wird.
        nOpenParaToken = static_cast< sal_uInt16 >(nToken);
    }

    SwTxtNode* pTxtNode = pPam->GetNode().GetTxtNode();
    ((SwCntntNode *)pTxtNode)->SetAttr( SwNumRuleItem(aNumRuleName) );
    pTxtNode->SetAttrListLevel(nLevel);
    // #i57656# - <IsCounted()> state of text node has to be adjusted accordingly.
    if ( nLevel < MAXLEVEL )
    {
        pTxtNode->SetCountedInList( bCountedInList );
    }
    // #i57919#
    // correction of refactoring done by cws swnumtree
    // - <nStart> contains the start value, if the numbering has to be restarted
    //   at this text node. Value <USHRT_MAX> indicates, that numbering isn't
    //   restarted at this text node
    if ( nStart != USHRT_MAX )
    {
        pTxtNode->SetListRestart( true );
        pTxtNode->SetAttrListRestartValue( nStart );
    }

    if( GetNumInfo().GetNumRule() )
        GetNumInfo().GetNumRule()->SetInvalidRule( true );

    // Styles parsen
    if( HasStyleOptions( aStyle, aId, aClass, &aLang, &aDir ) )
    {
        SfxItemSet aItemSet( pDoc->GetAttrPool(), pCSS1Parser->GetWhichMap() );
        SvxCSS1PropertyInfo aPropInfo;

        if( ParseStyleOptions( aStyle, aId, aClass, aItemSet, aPropInfo, &aLang, &aDir ) )
        {
            DoPositioning( aItemSet, aPropInfo, pCntxt );
            InsertAttrs( aItemSet, aPropInfo, pCntxt );
        }
    }

    PushContext( pCntxt );

    // die neue Vorlage setzen
    SetTxtCollAttrs( pCntxt );

    // Laufbalkenanzeige aktualisieren
    ShowStatline();
}

void SwHTMLParser::EndNumBulListItem( int nToken, bool bSetColl,
                                      bool /*bLastPara*/ )
{
    // einen neuen Absatz aufmachen
    if( !nToken && pPam->GetPoint()->nContent.GetIndex() )
        AppendTxtNode( AM_NOSPACE );

    // Kontext zu dem Token suchen und vom Stack holen
    _HTMLAttrContext *pCntxt = 0;
    sal_uInt16 nPos = aContexts.size();
    nToken &= ~1;
    while( !pCntxt && nPos>nContextStMin )
    {
        sal_uInt16 nCntxtToken = aContexts[--nPos]->GetToken();
        switch( nCntxtToken )
        {
        case HTML_LI_ON:
        case HTML_LISTHEADER_ON:
            if( !nToken || nToken == nCntxtToken  )
            {
                pCntxt = aContexts[nPos];
                aContexts.erase( aContexts.begin() + nPos );
            }
            break;
        case HTML_ORDERLIST_ON:
        case HTML_UNORDERLIST_ON:
        case HTML_MENULIST_ON:
        case HTML_DIRLIST_ON:
            // keine LI/LH ausserhalb der aktuellen Liste betrachten
            nPos = nContextStMin;
            break;
        }
    }

    // und noch Attribute beenden
    if( pCntxt )
    {
        EndContext( pCntxt );
        SetAttr();  // Absatz-Atts wegen JavaScript moeglichst schnell setzen
        delete pCntxt;
    }

    // und die bisherige Vorlage setzen
    if( bSetColl )
        SetTxtCollAttrs();
}

void SwHTMLParser::SetNodeNum( sal_uInt8 nLevel, bool bCountedInList )
{
    SwTxtNode* pTxtNode = pPam->GetNode().GetTxtNode();
    OSL_ENSURE( pTxtNode, "Kein Text-Node an PaM-Position" );

    OSL_ENSURE( GetNumInfo().GetNumRule(), "Kein Numerierungs-Regel" );
    const OUString& rName = GetNumInfo().GetNumRule()->GetName();
    ((SwCntntNode *)pTxtNode)->SetAttr( SwNumRuleItem(rName) );

    pTxtNode->SetAttrListLevel( nLevel );
    pTxtNode->SetCountedInList( bCountedInList );

    // NumRule invalidieren, weil sie durch ein EndAction bereits
    // auf valid geschaltet worden sein kann.
    GetNumInfo().GetNumRule()->SetInvalidRule( false );
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
