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

#include <com/sun/star/i18n/ScriptType.hpp>
#include <comphelper/string.hxx>
#include <svtools/htmlkywd.hxx>
#include <svtools/htmlout.hxx>
#include <svtools/htmltokn.h>
#include <fmtfld.hxx>
#include <doc.hxx>
#include <breakit.hxx>
#include <ndtxt.hxx>
#include <txtfld.hxx>
#include "fldbas.hxx"
#include "docufld.hxx"
#include "flddat.hxx"
#include "htmlfld.hxx"
#include "wrthtml.hxx"
#include <rtl/strbuf.hxx>
#include <css1atr.hxx>

using namespace nsSwDocInfoSubType;

const sal_Char *SwHTMLWriter::GetNumFormat( sal_uInt16 nFmt )
{
    const sal_Char *pFmtStr = 0;

    switch( (SvxExtNumType)nFmt )
    {
    case SVX_NUM_CHARS_UPPER_LETTER:    pFmtStr = OOO_STRING_SW_HTML_FF_uletter;    break;
    case SVX_NUM_CHARS_LOWER_LETTER:    pFmtStr = OOO_STRING_SW_HTML_FF_lletter;    break;
    case SVX_NUM_ROMAN_UPPER:           pFmtStr = OOO_STRING_SW_HTML_FF_uroman;         break;
    case SVX_NUM_ROMAN_LOWER:           pFmtStr = OOO_STRING_SW_HTML_FF_lroman;         break;
    case SVX_NUM_ARABIC:                pFmtStr = OOO_STRING_SW_HTML_FF_arabic;     break;
    case SVX_NUM_NUMBER_NONE:           pFmtStr = OOO_STRING_SW_HTML_FF_none;       break;
    case SVX_NUM_CHAR_SPECIAL:          pFmtStr = OOO_STRING_SW_HTML_FF_char;       break;
    case SVX_NUM_PAGEDESC:              pFmtStr = OOO_STRING_SW_HTML_FF_page;       break;
    case SVX_NUM_CHARS_UPPER_LETTER_N:  pFmtStr = OOO_STRING_SW_HTML_FF_ulettern;   break;
    case SVX_NUM_CHARS_LOWER_LETTER_N:  pFmtStr = OOO_STRING_SW_HTML_FF_llettern;   break;
    default:
        ;
    }

    return pFmtStr;
}

static Writer& OutHTML_SwField( Writer& rWrt, const SwField* pFld,
                                 const SwTxtNode& rTxtNd, sal_Int32 nFldPos )
{
    SwHTMLWriter & rHTMLWrt = (SwHTMLWriter&)rWrt;

    const SwFieldType* pFldTyp = pFld->GetTyp();
    sal_uInt16 nField = pFldTyp->Which();
    sal_uLong nFmt = pFld->GetFormat();

    const sal_Char *pTypeStr=0, // TYPE
                      *pSubStr=0,   // SUBTYPE
                   *pFmtStr=0;  // FORMAT (SW)
    OUString aValue;              // VALUE (SW)
    bool bNumFmt=false;         // SDNUM (Number-Formatter-Format)
    bool bNumValue=false;       // SDVAL (Number-Formatter-Value)
    double dNumValue = 0.0;     // SDVAL (Number-Formatter-Value)
    bool bFixed=false;          // SDFIXED
    OUString aName;               // NAME (CUSTOM)

    switch( nField )
    {
        case RES_EXTUSERFLD:
            pTypeStr = OOO_STRING_SW_HTML_FT_sender;
            switch( (SwExtUserSubType)pFld->GetSubType() )
            {
                case EU_COMPANY:    pSubStr = OOO_STRING_SW_HTML_FS_company;        break;
                case EU_FIRSTNAME:  pSubStr = OOO_STRING_SW_HTML_FS_firstname;  break;
                case EU_NAME:       pSubStr = OOO_STRING_SW_HTML_FS_name;       break;
                case EU_SHORTCUT:   pSubStr = OOO_STRING_SW_HTML_FS_shortcut;   break;
                case EU_STREET:     pSubStr = OOO_STRING_SW_HTML_FS_street;     break;
                case EU_COUNTRY:    pSubStr = OOO_STRING_SW_HTML_FS_country;     break;
                case EU_ZIP:        pSubStr = OOO_STRING_SW_HTML_FS_zip;         break;
                case EU_CITY:       pSubStr = OOO_STRING_SW_HTML_FS_city;        break;
                case EU_TITLE:      pSubStr = OOO_STRING_SW_HTML_FS_title;       break;
                case EU_POSITION:   pSubStr = OOO_STRING_SW_HTML_FS_position;    break;
                case EU_PHONE_PRIVATE:  pSubStr = OOO_STRING_SW_HTML_FS_pphone;      break;
                case EU_PHONE_COMPANY:  pSubStr = OOO_STRING_SW_HTML_FS_cphone;      break;
                case EU_FAX:        pSubStr = OOO_STRING_SW_HTML_FS_fax;         break;
                case EU_EMAIL:      pSubStr = OOO_STRING_SW_HTML_FS_email;       break;
                case EU_STATE:      pSubStr = OOO_STRING_SW_HTML_FS_state;       break;
                default:
                    ;
            }
            OSL_ENSURE( pSubStr, "ubekannter Subtyp fuer SwExtUserField" );
            bFixed = ((const SwExtUserField*)pFld)->IsFixed();
            break;

        case RES_AUTHORFLD:
            pTypeStr = OOO_STRING_SW_HTML_FT_author;
            switch( (SwAuthorFormat)nFmt & 0xff)
            {
                case AF_NAME:     pFmtStr = OOO_STRING_SW_HTML_FF_name;     break;
                case AF_SHORTCUT:  pFmtStr = OOO_STRING_SW_HTML_FF_shortcut;    break;
            }
            OSL_ENSURE( pFmtStr, "ubekanntes Format fuer SwAuthorField" );
            bFixed = ((const SwAuthorField*)pFld)->IsFixed();
            break;

        case RES_DATETIMEFLD:
            pTypeStr = OOO_STRING_SW_HTML_FT_datetime;
            bNumFmt = true;
            if( ((SwDateTimeField*)pFld)->IsFixed() )
            {
                bNumValue = true;
                dNumValue = ((SwDateTimeField*)pFld)->GetValue();
            }
            break;

        case RES_PAGENUMBERFLD:
            {
                pTypeStr = OOO_STRING_SW_HTML_FT_page;
                SwPageNumSubType eSubType = (SwPageNumSubType)pFld->GetSubType();
                switch( eSubType )
                {
                    case PG_RANDOM:     pSubStr = OOO_STRING_SW_HTML_FS_random;     break;
                    case PG_NEXT:       pSubStr = OOO_STRING_SW_HTML_FS_next;       break;
                    case PG_PREV:       pSubStr = OOO_STRING_SW_HTML_FS_prev;       break;
                }
                OSL_ENSURE( pSubStr, "ubekannter Subtyp fuer SwPageNumberField" );
                pFmtStr = SwHTMLWriter::GetNumFormat( static_cast< sal_uInt16 >(nFmt) );

                if( (SvxExtNumType)nFmt==SVX_NUM_CHAR_SPECIAL )
                {
                    aValue = ((const SwPageNumberField *)pFld)->GetUserString();
                }
                else
                {
                    const OUString& rValue = pFld->GetPar2();
                    short nValue = (short)rValue.toInt32();
                    if( (eSubType == PG_NEXT && nValue!=1) ||
                        (eSubType == PG_PREV && nValue!=-1) ||
                        (eSubType == PG_RANDOM && nValue!=0) )
                    {
                        aValue = rValue;
                    }
                }
            }
            break;
        case RES_DOCINFOFLD:
            {
                sal_uInt16 nSubType = pFld->GetSubType();
                pTypeStr = OOO_STRING_SW_HTML_FT_docinfo;
                sal_uInt16 nExtSubType = nSubType & 0x0f00;
                nSubType &= 0x00ff;

                switch( nSubType )
                {
                    case DI_TITEL:      pSubStr = OOO_STRING_SW_HTML_FS_title;  break;
                    case DI_THEMA:      pSubStr = OOO_STRING_SW_HTML_FS_theme;  break;
                    case DI_KEYS:       pSubStr = OOO_STRING_SW_HTML_FS_keys;   break;
                    case DI_COMMENT:    pSubStr = OOO_STRING_SW_HTML_FS_comment; break;
                    case DI_CREATE:     pSubStr = OOO_STRING_SW_HTML_FS_create;     break;
                    case DI_CHANGE:     pSubStr = OOO_STRING_SW_HTML_FS_change;     break;
                    case DI_CUSTOM:     pSubStr = OOO_STRING_SW_HTML_FS_custom;     break;
                    default:            pTypeStr = 0;               break;
                }

                if( DI_CUSTOM == nSubType ) {
                    aName = static_cast<const SwDocInfoField*>(pFld)->GetName();
                }

                if( DI_CREATE == nSubType || DI_CHANGE == nSubType )
                {
                    switch( nExtSubType )
                    {
                        case DI_SUB_AUTHOR:
                            pFmtStr = OOO_STRING_SW_HTML_FF_author;
                            break;
                        case DI_SUB_TIME:
                            pFmtStr = OOO_STRING_SW_HTML_FF_time;
                            bNumFmt = true;
                            break;
                        case DI_SUB_DATE:
                            pFmtStr = OOO_STRING_SW_HTML_FF_date;
                            bNumFmt = true;
                            break;
                    }
                }
                bFixed = ((const SwDocInfoField*)pFld)->IsFixed();
                if( bNumFmt )
                {
                    if( bFixed )
                    {
                        // Fuer ein fixes Feld och den Num-Value ausgeben.
                        // Fixe Felder ohne Zahlenformate sollte es
                        // eigentlich nicht geben. OSL_ENSURE(ist unten.
                        dNumValue = ((const SwDocInfoField*)pFld)->GetValue();
                        bNumValue = true;
                    }
                    else if( !nFmt  )
                    {
                        // Nicht fixe Felder muessen kein Zahlenformat haben,
                        // wenn sie aus 4.0-Dokumenten stammen.
                        bNumFmt = false;
                    }
                }
            }
            break;

        case RES_DOCSTATFLD:
            {
                pTypeStr = OOO_STRING_SW_HTML_FT_docstat;
                sal_uInt16 nSubType = pFld->GetSubType();
                switch( nSubType )
                {
                    case DS_PAGE:       pSubStr = OOO_STRING_SW_HTML_FS_page;   break;
                    case DS_PARA:       pSubStr = OOO_STRING_SW_HTML_FS_para;   break;
                    case DS_WORD:       pSubStr = OOO_STRING_SW_HTML_FS_word;   break;
                    case DS_CHAR:       pSubStr = OOO_STRING_SW_HTML_FS_char;   break;
                    case DS_TBL:        pSubStr = OOO_STRING_SW_HTML_FS_tbl;    break;
                    case DS_GRF:        pSubStr = OOO_STRING_SW_HTML_FS_grf;    break;
                    case DS_OLE:        pSubStr = OOO_STRING_SW_HTML_FS_ole;    break;
                    default:            pTypeStr = 0;               break;
                }
                pFmtStr = SwHTMLWriter::GetNumFormat( static_cast< sal_uInt16 >(nFmt) );
            }
            break;

        case RES_FILENAMEFLD:
            pTypeStr = OOO_STRING_SW_HTML_FT_filename;
            switch( (SwFileNameFormat)(nFmt & ~FF_FIXED) )
            {
                case FF_NAME:       pFmtStr = OOO_STRING_SW_HTML_FF_name;       break;
                case FF_PATHNAME:   pFmtStr = OOO_STRING_SW_HTML_FF_pathname;   break;
                case FF_PATH:       pFmtStr = OOO_STRING_SW_HTML_FF_path;       break;
                case FF_NAME_NOEXT: pFmtStr = OOO_STRING_SW_HTML_FF_name_noext; break;
                default:
                    ;
            }
            bFixed = ((const SwFileNameField*)pFld)->IsFixed();
            OSL_ENSURE( pFmtStr, "unbekanntes Format fuer SwFileNameField" );
            break;
    }

    // <SDFIELD>-Tag ausgeben
    if( pTypeStr )
    {
        OStringBuffer sOut;
        sOut.append('<');
        sOut.append(OOO_STRING_SVTOOLS_HTML_sdfield).append(' ').
            append(OOO_STRING_SVTOOLS_HTML_O_type).append('=').
            append(pTypeStr);
        if( pSubStr )
        {
            sOut.append(' ').append(OOO_STRING_SVTOOLS_HTML_O_subtype).
                append('=').append(pSubStr);
        }
        if( pFmtStr )
        {
            sOut.append(' ').append(OOO_STRING_SVTOOLS_HTML_O_format).
                append('=').append(pFmtStr);
        }
        if( !aName.isEmpty() )
        {
            sOut.append(' ').append(OOO_STRING_SVTOOLS_HTML_O_name).
                append("=\"");
            rWrt.Strm().WriteCharPtr( sOut.makeStringAndClear().getStr() );
            HTMLOutFuncs::Out_String( rWrt.Strm(), aName, rHTMLWrt.eDestEnc, &rHTMLWrt.aNonConvertableCharacters );
            sOut.append('\"');
        }
        if( !aValue.isEmpty() )
        {
            sOut.append(' ').append(OOO_STRING_SVTOOLS_HTML_O_value).
                append("=\"");
            rWrt.Strm().WriteCharPtr( sOut.makeStringAndClear().getStr() );
            HTMLOutFuncs::Out_String( rWrt.Strm(), aValue, rHTMLWrt.eDestEnc, &rHTMLWrt.aNonConvertableCharacters );
            sOut.append('\"');
        }
        if( bNumFmt )
        {
            OSL_ENSURE( nFmt, "Zahlenformat ist 0" );
            sOut.append(HTMLOutFuncs::CreateTableDataOptionsValNum(
                bNumValue, dNumValue, nFmt,
                *rHTMLWrt.pDoc->GetNumberFormatter(), rHTMLWrt.eDestEnc,
                &rHTMLWrt.aNonConvertableCharacters));
        }
        if( bFixed )
        {
            sOut.append(' ').append(OOO_STRING_SVTOOLS_HTML_O_sdfixed);
        }
        sOut.append('>');
        rWrt.Strm().WriteCharPtr( sOut.makeStringAndClear().getStr() );
    }

    // Inhalt des Feldes ausgeben
    OUString const sExpand( pFld->ExpandField(true) );
    bool bNeedsCJKProcessing = false;
    if( !sExpand.isEmpty() )
    {
        sal_uInt16 nScriptType = g_pBreakIt->GetBreakIter()->getScriptType( sExpand, 0 );
        sal_Int32 nPos = g_pBreakIt->GetBreakIter()->endOfScript( sExpand, 0,
                                                          nScriptType );

        sal_uInt16 nScript =
            SwHTMLWriter::GetCSS1ScriptForScriptType( nScriptType );
        if( (nPos < sExpand.getLength() && nPos >= 0) || nScript != rHTMLWrt.nCSS1Script )
        {
            bNeedsCJKProcessing = true;
        }
    }

    if( bNeedsCJKProcessing )
    {
        //sequence of (start, end) property ranges we want to
        //query
        SfxItemSet aScriptItemSet( rWrt.pDoc->GetAttrPool(),
                                   RES_CHRATR_FONT, RES_CHRATR_FONTSIZE,
                                   RES_CHRATR_POSTURE, RES_CHRATR_POSTURE,
                                   RES_CHRATR_WEIGHT, RES_CHRATR_WEIGHT,
                                   RES_CHRATR_CJK_FONT, RES_CHRATR_CTL_WEIGHT,
                                   0 );
        rTxtNd.GetAttr( aScriptItemSet, nFldPos, nFldPos+1 );

        sal_uInt16 aWesternWhichIds[4] =
            { RES_CHRATR_FONT, RES_CHRATR_FONTSIZE,
              RES_CHRATR_POSTURE, RES_CHRATR_WEIGHT };
        sal_uInt16 aCJKWhichIds[4] =
            { RES_CHRATR_CJK_FONT, RES_CHRATR_CJK_FONTSIZE,
              RES_CHRATR_CJK_POSTURE, RES_CHRATR_CJK_WEIGHT };
        sal_uInt16 aCTLWhichIds[4] =
            { RES_CHRATR_CTL_FONT, RES_CHRATR_CTL_FONTSIZE,
              RES_CHRATR_CTL_POSTURE, RES_CHRATR_CTL_WEIGHT };

        sal_uInt16 *pRefWhichIds = 0;
        switch( rHTMLWrt.nCSS1Script )
        {
        case CSS1_OUTMODE_WESTERN:
            pRefWhichIds = aWesternWhichIds;
            break;
        case CSS1_OUTMODE_CJK:
            pRefWhichIds = aCJKWhichIds;
            break;
        case CSS1_OUTMODE_CTL:
            pRefWhichIds = aCTLWhichIds;
            break;
        }

        sal_Int32 nPos = 0;
        do
        {
            sal_uInt16 nScriptType = g_pBreakIt->GetBreakIter()->getScriptType( sExpand, nPos );
            sal_uInt16 nScript =
                SwHTMLWriter::GetCSS1ScriptForScriptType( nScriptType );
            sal_Int32 nEndPos = g_pBreakIt->GetBreakIter()->endOfScript(
                                    sExpand, nPos, nScriptType );
            sal_Int32 nChunkLen = nEndPos - nPos;
            if( nScript != CSS1_OUTMODE_ANY_SCRIPT &&
                /* #108791# */ nScript != rHTMLWrt.nCSS1Script )
            {
                sal_uInt16 *pWhichIds = 0;
                switch( nScript )
                {
                case CSS1_OUTMODE_WESTERN:  pWhichIds = aWesternWhichIds; break;
                case CSS1_OUTMODE_CJK:      pWhichIds = aCJKWhichIds; break;
                case CSS1_OUTMODE_CTL:      pWhichIds = aCTLWhichIds; break;
                }

                rHTMLWrt.bTagOn = true;

                const SfxPoolItem *aItems[5];
                sal_uInt16 nItems = 0;

                assert(pWhichIds && pRefWhichIds);
                if (pWhichIds && pRefWhichIds)
                {
                    for( sal_uInt16 i=0; i<4; i++ )
                    {
                        const SfxPoolItem *pRefItem =
                            aScriptItemSet.GetItem( pRefWhichIds[i] );
                        const SfxPoolItem *pItem =
                            aScriptItemSet.GetItem( pWhichIds[i] );
                        if( pRefItem && pItem &&
                            !(0==i ? swhtml_css1atr_equalFontItems( *pRefItem, *pItem )
                                   : *pRefItem == *pItem) )
                        {
                            Out( aHTMLAttrFnTab, *pItem, rHTMLWrt );
                            aItems[nItems++] = pItem;
                        }
                    }
                }

                HTMLOutFuncs::Out_String( rWrt.Strm(), sExpand.copy( nPos, nChunkLen ),
                    rHTMLWrt.eDestEnc, &rHTMLWrt.aNonConvertableCharacters );

                rHTMLWrt.bTagOn = false;
                while( nItems )
                    Out( aHTMLAttrFnTab, *aItems[--nItems], rHTMLWrt );

            }
            else
            {
                HTMLOutFuncs::Out_String( rWrt.Strm(), sExpand.copy( nPos, nChunkLen ),
                    rHTMLWrt.eDestEnc, &rHTMLWrt.aNonConvertableCharacters );
            }
            nPos = nEndPos;
        }
        while( nPos < sExpand.getLength() );
    }
    else
    {
        HTMLOutFuncs::Out_String( rWrt.Strm(), sExpand,
              rHTMLWrt.eDestEnc, &rHTMLWrt.aNonConvertableCharacters );
    }

    // Off-Tag ausgeben
    if( pTypeStr )
        HTMLOutFuncs::Out_AsciiTag( rWrt.Strm(), OOO_STRING_SVTOOLS_HTML_sdfield, false );

    return rWrt;
}

Writer& OutHTML_SwFmtFld( Writer& rWrt, const SfxPoolItem& rHt )
{
    SwFmtFld & rFld = (SwFmtFld&)rHt;
    const SwField* pFld = rFld.GetField();
    const SwFieldType* pFldTyp = pFld->GetTyp();

    if( RES_SETEXPFLD == pFldTyp->Which() &&
        (nsSwGetSetExpType::GSE_STRING & pFld->GetSubType()) )
    {
        int bOn = sal_False;
        if (pFldTyp->GetName() == "HTML_ON")
            bOn = sal_True;
        else if (pFldTyp->GetName() != "HTML_OFF")
            return rWrt;

        OUString rTxt(comphelper::string::strip(pFld->GetPar2(), ' '));
        rWrt.Strm().WriteChar( '<' );
        if( !bOn )
            rWrt.Strm().WriteChar( '/' );
        // TODO: HTML-Tags are written without entitities, that for, characters
        // not contained in the destination encoding are lost!
        OString sTmp(OUStringToOString(rTxt,
            ((SwHTMLWriter&)rWrt).eDestEnc));
        rWrt.Strm().WriteCharPtr( sTmp.getStr() ).WriteChar( '>' );
    }
    else if( RES_POSTITFLD == pFldTyp->Which() )
    {
        // Kommentare werden im ANSI-Zeichensetz, aber mit System-Zeilen-
        // Umbruechen gesschrieben.
        const OUString& rComment = pFld->GetPar2();
        bool bWritten = false;

        if( (rComment.getLength() >= 6 && rComment.startsWith("<") && rComment.endsWith(">") &&
             rComment.copy( 1, 4 ).equalsIgnoreAsciiCase( OOO_STRING_SVTOOLS_HTML_meta) ) ||
            (rComment.getLength() >= 7 &&
             rComment.startsWith( "<!--" ) &&
             rComment.endsWith( "-->" )) )
        {
            // META-Tags direkt ausgeben
            OUString sComment(convertLineEnd(rComment, GetSystemLineEnd()));
            // TODO: HTML-Tags are written without entitities, that for,
            // characters not contained in the destination encoding are lost!
            OString sTmp(OUStringToOString(sComment,
                ((SwHTMLWriter&)rWrt).eDestEnc));
            rWrt.Strm().WriteCharPtr( sTmp.getStr() );
            bWritten = true;
        }
        else if( rComment.getLength() >= 7 &&
                 rComment.endsWith(">") &&
                 rComment.startsWithIgnoreAsciiCase( "HTML:" ) )
        {
            OUString sComment(comphelper::string::stripStart(rComment.copy(5), ' '));
            if( '<' == sComment[0] )
            {
                sComment = convertLineEnd(sComment, GetSystemLineEnd());
                // TODO: HTML-Tags are written without entitities, that for,
                // characters not contained in the destination encoding are
                // lost!
                OString sTmp(OUStringToOString(sComment,
                    ((SwHTMLWriter&)rWrt).eDestEnc));
                rWrt.Strm().WriteCharPtr( sTmp.getStr() );
                bWritten = true;
            }

        }

        if( !bWritten )
        {
            OUString sComment(convertLineEnd(rComment, GetSystemLineEnd()));
            OStringBuffer sOut;
            // TODO: ???
            sOut.append('<').append(OOO_STRING_SVTOOLS_HTML_comment)
                .append(' ').append(OUStringToOString(sComment,
                    ((SwHTMLWriter&)rWrt).eDestEnc)).append(" -->");
            rWrt.Strm().WriteCharPtr( sOut.getStr() );
        }
    }
    else if( RES_SCRIPTFLD == pFldTyp->Which() )
    {
        SwHTMLWriter& rHTMLWrt = (SwHTMLWriter&)rWrt;
        if( rHTMLWrt.bLFPossible )
            rHTMLWrt.OutNewLine( true );

        bool bURL = ((const SwScriptField *)pFld)->IsCodeURL();
        const OUString& rType = pFld->GetPar1();
        OUString aContents, aURL;
        if(bURL)
            aURL = pFld->GetPar2();
        else
            aContents = pFld->GetPar2();

        // sonst ist es der Script-Inhalt selbst. Da nur noh JavaScript
        // in Feldern landet, muss es sich um JavaSrript handeln ...:)
        HTMLOutFuncs::OutScript( rWrt.Strm(), rWrt.GetBaseURL(), aContents, rType, JAVASCRIPT,
                                 aURL, 0, 0, rHTMLWrt.eDestEnc, &rHTMLWrt.aNonConvertableCharacters );

        if( rHTMLWrt.bLFPossible )
            rHTMLWrt.OutNewLine( true );
    }
    else
    {
        const SwTxtFld *pTxtFld = rFld.GetTxtFld();
        OSL_ENSURE( pTxtFld, "Where is the txt fld?" );
        if( pTxtFld )
            OutHTML_SwField( rWrt, pFld, pTxtFld->GetTxtNode(),
                             pTxtFld->GetStart()  );
    }
    return rWrt;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
