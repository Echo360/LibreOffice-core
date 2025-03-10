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

#include <config_features.h>

#include <sal/types.h>
#include <tools/solar.h>
#include <comphelper/processfactory.hxx>
#include <comphelper/storagehelper.hxx>
#include <comphelper/string.hxx>
#include <sot/storinfo.hxx>
#include <com/sun/star/embed/XStorage.hpp>
#include <com/sun/star/embed/ElementModes.hpp>
#include <com/sun/star/embed/XTransactedObject.hpp>
#include <com/sun/star/io/XStream.hpp>

#include <com/sun/star/ucb/XCommandEnvironment.hpp>
#include <svl/urihelper.hxx>
#include <svl/zforlist.hxx>
#include <svl/zformat.hxx>
#include <sfx2/linkmgr.hxx>

#include <ucbhelper/content.hxx>

#include <com/sun/star/i18n/ScriptType.hpp>
#include <hintids.hxx>
#include <editeng/fontitem.hxx>
#include <editeng/fhgtitem.hxx>
#include <editeng/langitem.hxx>
#include <fmtfld.hxx>
#include <fmtanchr.hxx>
#include <pam.hxx>
#include <doc.hxx>
#include <charatr.hxx>
#include <flddat.hxx>
#include <docufld.hxx>
#include <reffld.hxx>
#include <IMark.hxx>
#include <expfld.hxx>
#include <dbfld.hxx>
#include <usrfld.hxx>
#include <tox.hxx>
#include <section.hxx>
#include <ndtxt.hxx>
#include <fmtinfmt.hxx>
#include <chpfld.hxx>
#include <ftnidx.hxx>
#include <txtftn.hxx>
#include <viewsh.hxx>
#include <shellres.hxx>
#include <fmtruby.hxx>
#include <charfmt.hxx>
#include <txtatr.hxx>
#include <breakit.hxx>
#include <fmtclds.hxx>
#include <pagedesc.hxx>
#include <SwStyleNameMapper.hxx>

#include "ww8scan.hxx"
#include "ww8par.hxx"
#include "ww8par2.hxx"
#include "writerhelper.hxx"
#include "fields.hxx"
#include <unotools/fltrcfg.hxx>
#include <xmloff/odffields.hxx>

#include <algorithm>

#define MAX_FIELDLEN 64000

#define WW8_TOX_LEVEL_DELIM     ':'

using namespace ::com::sun::star;
using namespace msfilter::util;
using namespace sw::util;
using namespace sw::mark;
using namespace std; // #i24377#
using namespace nsSwDocInfoSubType;

//              Bookmarks

namespace
{
    // #120879# - helper method to identify a bookmark name to match the internal TOC bookmark naming convention
    bool IsTOCBookmarkName( const ::rtl::OUString& rName )
    {
        return rName.startsWith("_Toc");
    }
}

long SwWW8ImplReader::Read_Book(WW8PLCFManResult*)
{
    // should also work via pRes.nCo2OrIdx
    WW8PLCFx_Book* pB = pPlcxMan->GetBook();
    if( !pB )
    {
        OSL_ENSURE( pB, "WW8PLCFx_Book - Pointer does not exist" );
        return 0;
    }

    eBookStatus eB = pB->GetStatus();
    if (eB & BOOK_IGNORE)
        return 0;         // ignore bookmark

    if (pB->GetIsEnd())
    {
        pReffedStck->SetAttr(*pPaM->GetPoint(), RES_FLTR_BOOKMARK, true,
            pB->GetHandle(), (eB & BOOK_FIELD)!=0);
        return 0;
    }

    // "_Hlt*" are unnecessary
    const OUString* pName = pB->GetName();
    // Now, as we read the TOC field completely, we also need the hyperlinks inside keep available.
    // So the hidden bookmarks inside for hyperlink jumping also should be kept.
    if ( !pName ||
         pName->startsWithIgnoreAsciiCase( "_Hlt" ) )
    {
        return 0;
    }

    // do NOT call ToUpper as the bookmark name can also be a hyperlink target!

    OUString aVal;
    if( SwFltGetFlag( nFieldFlags, SwFltControlStack::BOOK_TO_VAR_REF ) )
    {
        // set variable for translation bookmark
        long nLen = pB->GetLen();
        if( nLen > MAX_FIELDLEN )
            nLen = MAX_FIELDLEN;

        long nOldPos = pStrm->Tell();
        nLen = pSBase->WW8ReadString( *pStrm, aVal, pB->GetStartPos(), nLen,
                                        eStructCharSet );
        pStrm->Seek( nOldPos );

        // now here the implementation of the old "QuoteString" and
        // I hope with a better performance as before. It's also only
        // needed if the filterflags say we will convert bookmarks
        // to SetExpFields! And this the exception!

        OUString sHex("\\x");
        bool bSetAsHex;
        bool bAllowCr = SwFltGetFlag(nFieldFlags,
            SwFltControlStack::ALLOW_FLD_CR) ? true : false;

        for( sal_Int32 nI = 0;
             nI < aVal.getLength() && aVal.getLength() < (MAX_FIELDLEN - 4);
             ++nI )
        {
            const sal_Unicode cChar = aVal[nI];
            switch( cChar )
            {
            case 0x0b:
            case 0x0c:
            case 0x0d:
                if( bAllowCr )
                    aVal = aVal.replaceAt( nI, 1, "\n" ), bSetAsHex = false;
                else
                    bSetAsHex = true;
                break;

            case 0xFE:
            case 0xFF:
                bSetAsHex = true;
                break;

            default:
                bSetAsHex = 0x20 > cChar;
                break;
            }

            if( bSetAsHex )
            {
                //all Hex-Numbers with \x before
                OUString sTmp( sHex );
                if( cChar < 0x10 )
                    sTmp += "0";
                sTmp += OUString::number( cChar, 16 );
                aVal = aVal.replaceAt( nI, 1 , sTmp );
                nI += sTmp.getLength() - 1;
            }
        }

        if ( aVal.getLength() > (MAX_FIELDLEN - 4))
            aVal = aVal.copy( 0, MAX_FIELDLEN - 4 );
    }

    //e.g. inserting bookmark around field result, so we need to put
    //it around the entire writer field, as we don't have the separation
    //of field and field result of word, see #i16941#
    SwPosition aStart(*pPaM->GetPoint());
    if (!maFieldStack.empty())
    {
        const WW8FieldEntry &rTest = maFieldStack.back();
        aStart = rTest.maStartPos;
    }

    const OUString sOrigName = BookmarkToWriter(*pName);
    pReffedStck->NewAttr( aStart,
                          SwFltBookmark( sOrigName, aVal, pB->GetHandle(), IsTOCBookmarkName( sOrigName ) ));
    return 0;
}

//    general help methods to separate parameters

/// translate FieldParameter names into the system character set and
/// at the same time, douple backslashes are converted into single ones
OUString SwWW8ImplReader::ConvertFFileName(const OUString& rOrg)
{
    OUString aName = rOrg;
    aName = aName.replaceAll("\\\\", OUString('\\'));
    aName = aName.replaceAll("%20", OUString(' '));

    // remove attached quotation marks
    if (aName.endsWith("\""))
        aName = aName.copy(0, aName.getLength()-1);

    // Need the more sophisticated url converter.
    if (!aName.isEmpty())
        aName = URIHelper::SmartRel2Abs(
            INetURLObject(sBaseURL), aName, Link(), false);

    return aName;
}

namespace
{
    /// translate FieldParameter names into the
    /// system character set and makes them uppercase
    void ConvertUFName( OUString& rName )
    {
        rName = GetAppCharClass().uppercase( rName );
    }
}

static void lcl_ConvertSequenceName(OUString& rSequenceName)
{
    ConvertUFName(rSequenceName);
    if ('0' <= rSequenceName[0] && '9' >= rSequenceName[0])
        rSequenceName = "_" + rSequenceName;
}

// FindParaStart() finds 1st Parameter that follows '\' and cToken
// and returns start of this parameter or -1
sal_Int32 FindParaStart( const OUString& rStr, sal_Unicode cToken, sal_Unicode cToken2 )
{
    bool bStr = false; // ignore inside a string

    for( sal_Int32 nBuf = 0; nBuf+1 < rStr.getLength(); nBuf++ )
    {
        if( rStr[ nBuf ] == '"' )
            bStr = !bStr;

        if(    !bStr
            && rStr[ nBuf ] == '\\'
            && (    rStr[ nBuf + 1 ] == cToken
                 || rStr[ nBuf + 1 ] == cToken2 ) )
        {
            nBuf += 2;
            // skip spaces between cToken and its parameters
            while(    nBuf < rStr.getLength()
                   && rStr[ nBuf ] == ' ' )
                nBuf++;
            // return start of parameters
            return nBuf < rStr.getLength() ? nBuf : -1;
        }
    }
    return -1;
}

// FindPara() findet den ersten Parameter mit '\' und cToken. Es wird
// ein neuer String allokiert ( der vom Aufrufer deallokiert werden muss )
// und alles, was zum Parameter gehoert, wird in ihm zurueckgeliefert.
OUString FindPara( const OUString& rStr, sal_Unicode cToken, sal_Unicode cToken2 )
{
    sal_Int32 n2;                                          // end
    sal_Int32 n = FindParaStart( rStr, cToken, cToken2 );  // start
    if( n == -1)
        return OUString();

    if(    rStr[ n ] == '"'
        || rStr[ n ] == 132 )
    {                               // Anfuehrungszeichen vor Para
        n++;                        // Anfuehrungszeichen ueberlesen
        n2 = n;                     // ab hier nach Ende suchen
        while(     n2 < rStr.getLength()
                && rStr[ n2 ] != 147
                && rStr[ n2 ] != '"' )
            n2++;                   // Ende d. Paras suchen
    }
    else
    {                           // keine Anfuehrungszeichen
        n2 = n;                     // ab hier nach Ende suchen
        while(     n2 < rStr.getLength()
                && rStr[ n2 ] != ' ' )
            n2++;                   // Ende d. Paras suchen
    }
    return rStr.copy( n, n2-n );
}

static SvxExtNumType GetNumTypeFromName(const OUString& rStr,
    bool bAllowPageDesc = false)
{
    SvxExtNumType eTyp = bAllowPageDesc ? SVX_NUM_PAGEDESC : SVX_NUM_ARABIC;
    if( rStr.startsWithIgnoreAsciiCase( "Arabi" ) )  // Arabisch, Arabic
        eTyp = SVX_NUM_ARABIC;
    else if( rStr.startsWith( "misch" ) )    // r"omisch
        eTyp = SVX_NUM_ROMAN_LOWER;
    else if( rStr.startsWith( "MISCH" ) )    // R"OMISCH
        eTyp = SVX_NUM_ROMAN_UPPER;
    else if( rStr.startsWithIgnoreAsciiCase( "alphabeti" ) )// alphabetisch, alphabetic
        eTyp =  ( rStr[0] == 'A' )
                ? SVX_NUM_CHARS_UPPER_LETTER_N
                : SVX_NUM_CHARS_LOWER_LETTER_N;
    else if( rStr.startsWithIgnoreAsciiCase( "roman" ) )  // us
        eTyp =  ( rStr[0] == 'R' )
                ? SVX_NUM_ROMAN_UPPER
                : SVX_NUM_ROMAN_LOWER;
    return eTyp;
}

static SvxExtNumType GetNumberPara(const OUString& rStr, bool bAllowPageDesc = false)
{
    OUString s( FindPara( rStr, '*', '*' ) );     // Ziffernart
    SvxExtNumType aType = GetNumTypeFromName( s, bAllowPageDesc );
    return aType;
}

bool SwWW8ImplReader::ForceFieldLanguage(SwField &rFld, sal_uInt16 nLang)
{
    bool bRet(false);

    const SvxLanguageItem *pLang =
        (const SvxLanguageItem*)GetFmtAttr(RES_CHRATR_LANGUAGE);
    OSL_ENSURE(pLang, "impossible");
    sal_uInt16 nDefault =  pLang ? pLang->GetValue() : LANGUAGE_ENGLISH_US;

    if (nLang != nDefault)
    {
        rFld.SetAutomaticLanguage(false);
        rFld.SetLanguage(nLang);
        bRet = true;
    }

    return bRet;
}

OUString GetWordDefaultDateStringAsUS(SvNumberFormatter* pFormatter, sal_uInt16 nLang)
{
    //Get the system date in the correct final language layout, convert to
    //a known language and modify the 2 digit year part to be 4 digit, and
    //convert back to the correct language layout.
    sal_uLong nIndex = pFormatter->GetFormatIndex(NF_DATE_SYSTEM_SHORT, nLang);

    SvNumberformat aFormat = const_cast<SvNumberformat &>
        (*(pFormatter->GetEntry(nIndex)));
    aFormat.ConvertLanguage(*pFormatter, nLang, LANGUAGE_ENGLISH_US);

    OUString sParams(aFormat.GetFormatstring());
    // #i36594#
    // Fix provided by mloiseleur@openoffice.org.
    // A default date can have already 4 year digits, in some case
    const sal_Int32 pos = sParams.indexOf("YYYY");
    if ( pos == -1 )
    {
        sParams = sParams.replaceFirst("YY", "YYYY");
    }
    return sParams;
}

short SwWW8ImplReader::GetTimeDatePara(OUString& rStr, sal_uInt32& rFormat,
    sal_uInt16 &rLang, int nWhichDefault, bool bHijri)
{
    bool bRTL = false;
    if (pPlcxMan && !bVer67)
    {
        const sal_uInt8 *pResult = pPlcxMan->HasCharSprm(0x85A);
        if (pResult && *pResult)
            bRTL = true;
    }
    RES_CHRATR eLang = bRTL ? RES_CHRATR_CTL_LANGUAGE : RES_CHRATR_LANGUAGE;
    const SvxLanguageItem *pLang = (SvxLanguageItem*)GetFmtAttr( static_cast< sal_uInt16 >(eLang));
    OSL_ENSURE(pLang, "impossible");
    rLang = pLang ? pLang->GetValue() : LANGUAGE_ENGLISH_US;

    SvNumberFormatter* pFormatter = rDoc.GetNumberFormatter();
    OUString sParams( FindPara( rStr, '@', '@' ) );// Date/Time
    if (sParams.isEmpty())
    {
        bool bHasTime = false;
        switch (nWhichDefault)
        {
            case ww::ePRINTDATE:
            case ww::eSAVEDATE:
                sParams = GetWordDefaultDateStringAsUS(pFormatter, rLang);
                sParams += " HH:MM:SS AM/PM";
                bHasTime = true;
                break;
            case ww::eCREATEDATE:
                sParams += "DD/MM/YYYY HH:MM:SS";
                bHasTime = true;
                break;
            default:
            case ww::eDATE:
                sParams = GetWordDefaultDateStringAsUS(pFormatter, rLang);
                break;
        }

        if (bHijri)
            sParams = "[~hijri]" + sParams;

        sal_Int32 nCheckPos = 0;
        short nType = NUMBERFORMAT_DEFINED;
        rFormat = 0;

        OUString sTemp(sParams);
        pFormatter->PutandConvertEntry(sTemp, nCheckPos, nType, rFormat,
                                       LANGUAGE_ENGLISH_US, rLang);
        sParams = sTemp;

        return bHasTime ? NUMBERFORMAT_DATETIME : NUMBERFORMAT_DATE;
    }

    sal_uLong nFmtIdx =
        sw::ms::MSDateTimeFormatToSwFormat(sParams, pFormatter, rLang, bHijri,
                GetFib().lid);
    short nNumFmtType = NUMBERFORMAT_UNDEFINED;
    if (nFmtIdx)
        nNumFmtType = pFormatter->GetType(nFmtIdx);
    rFormat = nFmtIdx;

    return nNumFmtType;
}

//              Felder

// Am Ende des Einlesens entsprechende Felder updaten ( z.Zt. die Referenzen )
void SwWW8ImplReader::UpdateFields()
{
    rDoc.SetUpdateExpFldStat(true);                 // JP: neu fuer alles wichtige
    rDoc.SetInitDBFields(true);             // Datenbank-Felder auch
}

sal_uInt16 SwWW8ImplReader::End_Field()
{
    sal_uInt16 nRet = 0;
    WW8PLCFx_FLD* pF = pPlcxMan->GetFld();
    OSL_ENSURE(pF, "WW8PLCFx_FLD - Pointer nicht da");
    WW8_CP nCP = 0;
    if (!pF || !pF->EndPosIsFieldEnd(nCP))
        return nRet;

    const SvtFilterOptions &rOpt = SvtFilterOptions::Get();
    bool bUseEnhFields = rOpt.IsUseEnhancedFields();

    OSL_ENSURE(!maFieldStack.empty(), "Empty field stack\n");
    if (!maFieldStack.empty())
    {
        /*
        only hyperlinks currently need to be handled like this, for the other
        cases we have inserted a field not an attribute with an unknown end
        point
        */
        nRet = maFieldStack.back().mnFieldId;
        switch (nRet)
        {
        case 70:
        if (bUseEnhFields && pPaM!=NULL && pPaM->GetPoint()!=NULL) {
            SwPosition aEndPos = *pPaM->GetPoint();
            SwPaM aFldPam( maFieldStack.back().GetPtNode(), maFieldStack.back().GetPtCntnt(), aEndPos.nNode, aEndPos.nContent.GetIndex());
            IDocumentMarkAccess* pMarksAccess = rDoc.getIDocumentMarkAccess( );
            IFieldmark *pFieldmark = dynamic_cast<IFieldmark*>( pMarksAccess->makeFieldBookmark(
                        aFldPam, maFieldStack.back().GetBookmarkName(), ODF_FORMTEXT ) );
            OSL_ENSURE(pFieldmark!=NULL, "hmmm; why was the bookmark not created?");
            if (pFieldmark!=NULL) {
                const IFieldmark::parameter_map_t& pParametersToAdd = maFieldStack.back().getParameters();
                pFieldmark->GetParameters()->insert(pParametersToAdd.begin(), pParametersToAdd.end());
            }
        }
        break;
            // Doing corresponding status management for TOX field, index field, hyperlink field and page reference field
            case 13://TOX
            case 8://index
                if (mbLoadingTOXCache)
                {
                    if (mnEmbeddedTOXLevel > 0)
                    {
                        JoinNode(*pPaM);
                        --mnEmbeddedTOXLevel;
                    }
                    else
                    {
                        maTOXEndCps.insert(nCP);
                        mbLoadingTOXCache = false;
                        if ( pPaM->End() &&
                             pPaM->End()->nNode.GetNode().GetTxtNode() &&
                             pPaM->End()->nNode.GetNode().GetTxtNode()->Len() == 0 )
                        {
                            JoinNode(*pPaM);
                        }
                        else
                        {
                            mbCareLastParaEndInToc = true;
                        }

                        if (mpPosAfterTOC)
                        {
                            *pPaM = *mpPosAfterTOC;
                            delete mpPosAfterTOC;
                            mpPosAfterTOC = 0;
                        }
                    }
                }
                break;
            case 37: //REF
                if (mbLoadingTOXCache && !mbLoadingTOXHyperlink)
                {
                    pCtrlStck->SetAttr(*pPaM->GetPoint(),RES_TXTATR_INETFMT);
                }
                break;
            case 88:
                if (mbLoadingTOXHyperlink)
                    mbLoadingTOXHyperlink = false;
                pCtrlStck->SetAttr(*pPaM->GetPoint(), RES_TXTATR_INETFMT);
                break;
            case 36:
            case 68:
                //Move outside the section associated with this type of field
                *pPaM->GetPoint() = maFieldStack.back().maStartPos;
                break;
            default:
                OUString aCode = maFieldStack.back().GetBookmarkCode();
                if ( !aCode.isEmpty() )
                {
                    // Unhandled field with stored code
                    SwPosition aEndPos = *pPaM->GetPoint();
                    SwPaM aFldPam(
                            maFieldStack.back().GetPtNode(), maFieldStack.back().GetPtCntnt(),
                            aEndPos.nNode, aEndPos.nContent.GetIndex());

                    IDocumentMarkAccess* pMarksAccess = rDoc.getIDocumentMarkAccess( );

                    IFieldmark* pFieldmark = pMarksAccess->makeFieldBookmark(
                                aFldPam,
                                maFieldStack.back().GetBookmarkName(),
                                ODF_UNHANDLED );
                    if ( pFieldmark )
                    {
                        const IFieldmark::parameter_map_t& pParametersToAdd = maFieldStack.back().getParameters();
                        pFieldmark->GetParameters()->insert(pParametersToAdd.begin(), pParametersToAdd.end());
                        OUString sFieldId = OUString::number( maFieldStack.back().mnFieldId );
                        pFieldmark->GetParameters()->insert(
                                std::pair< OUString, uno::Any > (
                                    ODF_ID_PARAM,
                                    uno::makeAny( sFieldId ) ) );
                        pFieldmark->GetParameters()->insert(
                                std::pair< OUString, uno::Any > (
                                    ODF_CODE_PARAM,
                                    uno::makeAny( aCode ) ) );

                        if ( maFieldStack.back().mnObjLocFc > 0 )
                        {
                            // Store the OLE object as an internal link
                            OUString sOleId('_');
                            sOleId += OUString::number( maFieldStack.back().mnObjLocFc );

                            SvStorageRef xSrc0 = pStg->OpenSotStorage(OUString(SL::aObjectPool));
                            SvStorageRef xSrc1 = xSrc0->OpenSotStorage( sOleId, STREAM_READ );

                            // Store it now!
                            uno::Reference< embed::XStorage > xDocStg = GetDoc().GetDocStorage();
                            if (xDocStg.is())
                            {
                                uno::Reference< embed::XStorage > xOleStg = xDocStg->openStorageElement(
                                        "OLELinks", embed::ElementModes::WRITE );
                                SotStorageRef xObjDst = SotStorage::OpenOLEStorage( xOleStg, sOleId );

                                if ( xObjDst.Is() )
                                {
                                    xSrc1->CopyTo( xObjDst );

                                    if ( !xObjDst->GetError() )
                                        xObjDst->Commit();
                                }

                                uno::Reference< embed::XTransactedObject > xTransact( xOleStg, uno::UNO_QUERY );
                                if ( xTransact.is() )
                                    xTransact->commit();
                            }

                            // Store the OLE Id as a parameter
                            pFieldmark->GetParameters()->insert(
                                    std::pair< OUString, uno::Any >(
                                        ODF_OLE_PARAM, uno::makeAny( OUString( sOleId ) ) ) );
                        }
                    }
                }

                break;
        }
        maFieldStack.pop_back();
    }
    return nRet;
}

bool AcceptableNestedField(sal_uInt16 nFieldCode)
{
    switch (nFieldCode)
    {
        case 8:  // allow recursive field in TOC...
        case 13: // allow recursive field in TOC...
        case 36:
        case 68:
        case 79:
        case 88:
        // Accept AutoTextList field as nested field.
        // Thus, the field result is imported as plain text.
        case 89:
            return true;
        default:
            return false;
    }
}

WW8FieldEntry::WW8FieldEntry(SwPosition &rPos, sal_uInt16 nFieldId) throw()
    : maStartPos(rPos), mnFieldId(nFieldId), mnObjLocFc(0)
{
}

WW8FieldEntry::WW8FieldEntry(const WW8FieldEntry &rOther) throw()
    : maStartPos(rOther.maStartPos), mnFieldId(rOther.mnFieldId), mnObjLocFc(rOther.mnObjLocFc)
{
}

void WW8FieldEntry::Swap(WW8FieldEntry &rOther) throw()
{
    std::swap(maStartPos, rOther.maStartPos);
    std::swap(mnFieldId, rOther.mnFieldId);
}

WW8FieldEntry &WW8FieldEntry::operator=(const WW8FieldEntry &rOther) throw()
{
    WW8FieldEntry aTemp(rOther);
    Swap(aTemp);
    return *this;
}



void WW8FieldEntry::SetBookmarkName(const OUString& bookmarkName)
{
    msBookmarkName=bookmarkName;
}

void WW8FieldEntry::SetBookmarkType(const OUString& bookmarkType)
{
    msMarkType=bookmarkType;
}

void WW8FieldEntry::SetBookmarkCode(const OUString& bookmarkCode)
{
    msMarkCode = bookmarkCode;
}


// Read_Field liest ein Feld ein oder, wenn es nicht gelesen werden kann,
// wird 0 zurueckgegeben, so dass das Feld vom Aufrufer textuell gelesen wird.
// Returnwert: Gesamtlaenge des Feldes ( zum UEberlesen )
long SwWW8ImplReader::Read_Field(WW8PLCFManResult* pRes)
{
    typedef eF_ResT (SwWW8ImplReader:: *FNReadField)( WW8FieldDesc*, OUString& );
    enum Limits {eMax = 96};
    static const FNReadField aWW8FieldTab[eMax+1] =
    {
        0,
        &SwWW8ImplReader::Read_F_Input,
        0,
        &SwWW8ImplReader::Read_F_Ref,               // 3
        0,
        0,
        &SwWW8ImplReader::Read_F_Set,               // 6
        0,
        &SwWW8ImplReader::Read_F_Tox,               // 8
        0,
        0,
        0,
        &SwWW8ImplReader::Read_F_Seq,               // 12
        &SwWW8ImplReader::Read_F_Tox,               // 13
        &SwWW8ImplReader::Read_F_DocInfo,           // 14
        &SwWW8ImplReader::Read_F_DocInfo,           // 15
        &SwWW8ImplReader::Read_F_DocInfo,           // 16
        &SwWW8ImplReader::Read_F_Author,            // 17
        &SwWW8ImplReader::Read_F_DocInfo,           // 18
        &SwWW8ImplReader::Read_F_DocInfo,           // 19
        &SwWW8ImplReader::Read_F_DocInfo,           // 20
        &SwWW8ImplReader::Read_F_DocInfo,           // 21
        &SwWW8ImplReader::Read_F_DocInfo,           // 22
        &SwWW8ImplReader::Read_F_DocInfo,           // 23
        &SwWW8ImplReader::Read_F_DocInfo,           // 24
        &SwWW8ImplReader::Read_F_DocInfo,           // 25
        &SwWW8ImplReader::Read_F_Anz,               // 26
        &SwWW8ImplReader::Read_F_Anz,               // 27
        &SwWW8ImplReader::Read_F_Anz,               // 28
        &SwWW8ImplReader::Read_F_FileName,          // 29
        &SwWW8ImplReader::Read_F_TemplName,         // 30
        &SwWW8ImplReader::Read_F_DateTime,          // 31
        &SwWW8ImplReader::Read_F_DateTime,          // 32
        &SwWW8ImplReader::Read_F_CurPage,           // 33
        0,
        0,
        &SwWW8ImplReader::Read_F_IncludeText,       // 36
        &SwWW8ImplReader::Read_F_PgRef,             // 37
        &SwWW8ImplReader::Read_F_InputVar,          // 38
        &SwWW8ImplReader::Read_F_Input,             // 39
        0,
        &SwWW8ImplReader::Read_F_DBNext,            // 41
        0,
        0,
        &SwWW8ImplReader::Read_F_DBNum,             // 44
        0,
        0,
        0,
        0,
        &SwWW8ImplReader::Read_F_Equation,          // 49
        0,
        &SwWW8ImplReader::Read_F_Macro,             // 51
        &SwWW8ImplReader::Read_F_ANumber,           // 52
        &SwWW8ImplReader::Read_F_ANumber,           // 53
        &SwWW8ImplReader::Read_F_ANumber,           // 54
        0,

        0,                                          // 56

        &SwWW8ImplReader::Read_F_Symbol,            // 57
        &SwWW8ImplReader::Read_F_Embedd,            // 58
        &SwWW8ImplReader::Read_F_DBField,           // 59
        0,
        0,
        0,
        0,
        &SwWW8ImplReader::Read_F_DocInfo,           // 64 - DOCVARIABLE
        0,
        0,
        &SwWW8ImplReader::Read_F_IncludePicture,    // 67
        &SwWW8ImplReader::Read_F_IncludeText,       // 68
        0,
        &SwWW8ImplReader::Read_F_FormTextBox,       // 70
        &SwWW8ImplReader::Read_F_FormCheckBox,      // 71
        &SwWW8ImplReader::Read_F_NoteReference,     // 72
        0, /*&SwWW8ImplReader::Read_F_Tox*/
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        &SwWW8ImplReader::Read_F_FormListBox,       // 83
        0,                                          // 84
        &SwWW8ImplReader::Read_F_DocInfo,           // 85
        0,                                          // 86
        &SwWW8ImplReader::Read_F_OCX,               // 87
        &SwWW8ImplReader::Read_F_Hyperlink,         // 88
        0,                                          // 89
        0,                                          // 90
        &SwWW8ImplReader::Read_F_HTMLControl,       // 91
        0,                                          // 92
        0,                                          // 93
        0,                                          // 94
        &SwWW8ImplReader::Read_F_Shape,             // 95
        0                                           // eMax - Dummy leer Methode
    };
    OSL_ENSURE( ( sizeof( aWW8FieldTab ) / sizeof( *aWW8FieldTab ) == eMax+1 ),
            "FeldFunc-Tabelle stimmt nicht" );

    WW8PLCFx_FLD* pF = pPlcxMan->GetFld();
    OSL_ENSURE(pF, "WW8PLCFx_FLD - Pointer nicht da");

    if (!pF || !pF->StartPosIsFieldStart())
        return 0;

    bool bNested = false;
    if (!maFieldStack.empty())
    {
        mycFieldIter aEnd = maFieldStack.end();
        for(mycFieldIter aIter = maFieldStack.begin(); aIter != aEnd; ++aIter)
        {
            bNested = !AcceptableNestedField(aIter->mnFieldId);
            if (bNested)
                break;
        }
    }

    WW8FieldDesc aF;
    bool bOk = pF->GetPara(pRes->nCp2OrIdx, aF);

    OSL_ENSURE(bOk, "WW8: Bad Field!\n");
    if (aF.nId == 33) aF.bCodeNest=false; // do not recurse into nested page fields
    bool bCodeNest = aF.bCodeNest;
    if ( aF.nId == 6 ) bCodeNest = false; // We can handle them and lose the inner data

    maFieldStack.push_back(WW8FieldEntry(*pPaM->GetPoint(), aF.nId));

    if (bNested)
        return 0;

    sal_uInt16 n = (aF.nId <= eMax) ? aF.nId : static_cast<sal_uInt16>(eMax);
    sal_uInt16 nI = n / 32;                     // # des sal_uInt32
    sal_uInt32 nMask = 1 << ( n % 32 );          // Maske fuer Bits

    if ((sizeof(nFieldTagAlways)/sizeof(nFieldTagAlways[0])) <= nI)
    {   // if indexes larger than 95 are needed, then a new configuration
        // item has to be added, and nFieldTagAlways/nFieldTagBad expanded!
        return aF.nLen;
    }

    if( nFieldTagAlways[nI] & nMask )       // Flag: Tag it
        return Read_F_Tag( &aF );           // Resultat nicht als Text

    if( !bOk || !aF.nId )                   // Feld kaputt
        return aF.nLen;                     // -> ignorieren

    if( aF.nId > eMax - 1)                        // WW: Nested Field
    {
        if( nFieldTagBad[nI] & nMask )      // Flag: Tag it when bad
            return Read_F_Tag( &aF );       // Resultat nicht als Text
        else
            return aF.nLen;
    }

    //Only one type of field (hyperlink) in drawing textboxes exists
    if (aF.nId != 88 && pPlcxMan->GetDoingDrawTextBox())
        return aF.nLen;

    // keine Routine vorhanden
    if (bNested || !aWW8FieldTab[aF.nId] || bCodeNest)
    {
        if( nFieldTagBad[nI] & nMask )      // Flag: Tag it when bad
            return Read_F_Tag( &aF );       // Resultat nicht als Text
                                            // Lese nur Resultat
        if (aF.bResNest && !AcceptableNestedField(aF.nId))
            return aF.nLen;                 // Result nested -> nicht brauchbar

        long nOldPos = pStrm->Tell();
        OUString aStr;
        aF.nLCode = pSBase->WW8ReadString( *pStrm, aStr, pPlcxMan->GetCpOfs()+
            aF.nSCode, aF.nLCode, eTextCharSet );
        pStrm->Seek( nOldPos );

        // field codes which contain '/' or '.' are not displayed in WinWord
        // skip if it is formula field or found space before. see #i119446, #i119585.
        const sal_Int32 nDotPos = aStr.indexOf('.');
        const sal_Int32 nSlashPos = aStr.indexOf('/');
        sal_Int32 nSpacePos = aStr.indexOf( ' ', 1 );
        if ( nSpacePos<0 )
            nSpacePos = aStr.getLength();

        if ( !( aStr.getLength()>1 && aStr[1]=='=') &&
            (( nDotPos>=0 && nDotPos < nSpacePos ) ||
             ( nSlashPos>=0 && nSlashPos < nSpacePos )))
            return aF.nLen;
        else
        {
            // Link fields aren't supported, but they are bound to an OLE object
            // that needs to be roundtripped
            if ( aF.nId == 56 )
                bEmbeddObj = true;
            // Field not supported: store the field code for later use
            maFieldStack.back().SetBookmarkCode( aStr );
            return aF.nLen - aF.nLRes - 1;  // so viele ueberlesen, das Resultfeld
                                            // wird wie Haupttext eingelesen
        }
    }
    else
    {                                   // Lies Feld
        long nOldPos = pStrm->Tell();
        OUString aStr;
        if ( aF.nId == 6 && aF.bCodeNest )
        {
            // TODO Extract the whole code string using the nested codes
            aF.nLCode = pSBase->WW8ReadString( *pStrm, aStr, pPlcxMan->GetCpOfs() +
                aF.nSCode, aF.nSRes - aF.nSCode - 1, eTextCharSet );
        }
        else
        {
            aF.nLCode = pSBase->WW8ReadString( *pStrm, aStr, pPlcxMan->GetCpOfs()+
                aF.nSCode, aF.nLCode, eTextCharSet );
        }

        // #i51312# - graphics inside field code not supported by Writer.
        // Thus, delete character 0x01, which stands for such a graphic.
        if (aF.nId==51) //#i56768# only do it for the MACROBUTTON field, since DropListFields need the 0x01.
        {
            aStr = aStr.replaceAll(OUString(0x01), "");
        }

        eF_ResT eRes = (this->*aWW8FieldTab[aF.nId])( &aF, aStr );
        pStrm->Seek( nOldPos );

        switch ( eRes )
        {
            case FLD_OK:
                return aF.nLen;                     // alles OK
            case FLD_TAGTXT:
                if ((nFieldTagBad[nI] & nMask)) // Flag: Tag bad
                    return Read_F_Tag(&aF);       // Taggen
                //fall through...
            case FLD_TEXT:
                // so viele ueberlesen, das Resultfeld wird wie Haupttext
                // eingelesen
                // attributes can start at char 0x14 so skip one
                // char more back == "-2"
                if (aF.nLRes)
                    return aF.nLen - aF.nLRes - 2;
                else
                    return aF.nLen;
            case FLD_TAGIGN:
                if(  ( nFieldTagBad[nI] & nMask ) ) // Flag: Tag bad
                    return Read_F_Tag( &aF );       // Taggen
                return aF.nLen;                 // oder ignorieren
            case FLD_READ_FSPA:
                return aF.nLen - aF.nLRes - 2; // auf Char 1 positionieren
            default:
                return aF.nLen;                     // ignorieren
        }
    }
}

//        Felder Taggen

// MakeTagString() gibt als Returnwert die Position des ersten
// CR / Zeilenende / Seitenumbruch in pText und wandelt auch nur bis dort
// Wenn keins dieser Sonderzeichen enthalten ist, wird 0 zurueckgeliefert.
void SwWW8ImplReader::MakeTagString( OUString& rStr, const OUString& rOrg )
{
    OUString sHex("\\x");
    bool bAllowCr = SwFltGetFlag( nFieldFlags, SwFltControlStack::TAGS_IN_TEXT )
                || SwFltGetFlag( nFieldFlags, SwFltControlStack::ALLOW_FLD_CR );
    sal_Unicode cChar;
    rStr = rOrg;

    for( sal_Int32 nI = 0;
            nI < rStr.getLength() && rStr.getLength() < (MAX_FIELDLEN - 4); ++nI )
    {
        bool bSetAsHex = false;
        switch( cChar = rStr[ nI ] )
        {
            case 132:                       // Typographische Anfuehrungszeichen
            case 148:                       // gegen normale tauschen
            case 147:
                rStr = rStr.replaceAt( nI, 1, "\"" );
                break;
            case 19:
                rStr = rStr.replaceAt( nI, 1, "{" );
                break;  // 19..21 zu {|}
            case 20:
                rStr = rStr.replaceAt( nI, 1, "|" );
                break;
            case 21:
                rStr = rStr.replaceAt( nI, 1, "}" );
                break;
            case '\\':                      // \{|} per \ Taggen
            case '{':
            case '|':
            case '}':
                rStr = rStr.replaceAt( nI, 0, "\\" );
                ++nI;
                break;
            case 0x0b:
            case 0x0c:
            case 0x0d:
                if( bAllowCr )
                    rStr = rStr.replaceAt( nI, 1, "\n" );
                else
                    bSetAsHex = true;
                break;
            case 0xFE:
            case 0xFF:
                bSetAsHex = true;
                break;
            default:
                bSetAsHex = 0x20 > cChar;
                break;
        }

        if( bSetAsHex )
        {
            //all Hex-Numbers with \x before
            OUString sTmp( sHex );
            if( cChar < 0x10 )
                sTmp += "0";
            sTmp += OUString::number( cChar, 16 );
            rStr = rStr.replaceAt( nI, 1 , sTmp );
            nI += sTmp.getLength() - 1;
        }
    }

    if( rStr.getLength() > (MAX_FIELDLEN - 4))
        rStr = rStr.copy( 0, MAX_FIELDLEN - 4 );
}

void SwWW8ImplReader::InsertTagField( const sal_uInt16 nId, const OUString& rTagText )
{
    OUString aName("WwFieldTag");
    if( SwFltGetFlag( nFieldFlags, SwFltControlStack::TAGS_DO_ID ) ) // Nummer?
        aName += OUString::number( nId );                    // ausgeben ?

    if( SwFltGetFlag(nFieldFlags, SwFltControlStack::TAGS_IN_TEXT))
    {
        aName += rTagText;      // als Txt taggen
        rDoc.getIDocumentContentOperations().InsertString(*pPaM, aName,
                IDocumentContentOperations::INS_NOHINTEXPAND);
    }
    else
    {                                                   // normal tagggen

        SwFieldType* pFT = rDoc.InsertFldType(
                                SwSetExpFieldType( &rDoc, aName, nsSwGetSetExpType::GSE_STRING ) );
        SwSetExpField aFld( (SwSetExpFieldType*)pFT, rTagText );                            // SUB_INVISIBLE
        sal_uInt16 nSubType = ( SwFltGetFlag( nFieldFlags, SwFltControlStack::TAGS_VISIBLE ) ) ? 0 : nsSwExtendedSubType::SUB_INVISIBLE;
        aFld.SetSubType(nSubType | nsSwGetSetExpType::GSE_STRING);

        rDoc.getIDocumentContentOperations().InsertPoolItem( *pPaM, SwFmtFld( aFld ), 0 );
    }
}

long SwWW8ImplReader::Read_F_Tag( WW8FieldDesc* pF )
{
    long nOldPos = pStrm->Tell();

    WW8_CP nStart = pF->nSCode - 1;         // mit 0x19 am Anfang
    long nL = pF->nLen;                     // Gesamtlaenge mit Resultat u. Nest
    if( nL > MAX_FIELDLEN )
        nL = MAX_FIELDLEN;                  // MaxLaenge, durch Quoten
                                            // max. 4* so gross
    OUString sFTxt;
    nL = pSBase->WW8ReadString( *pStrm, sFTxt,
                                pPlcxMan->GetCpOfs() + nStart, nL, eStructCharSet);

    OUString aTagText;
    MakeTagString( aTagText, sFTxt );
    InsertTagField( pF->nId, aTagText );

    pStrm->Seek( nOldPos );
    return pF->nLen;
}

//        normal fields

eF_ResT SwWW8ImplReader::Read_F_Input( WW8FieldDesc* pF, OUString& rStr )
{
    OUString aDef;
    OUString aQ;
    WW8ReadFieldParams aReadParam( rStr );
    for (;;)
    {
        const sal_Int32 nRet = aReadParam.SkipToNextToken();
        if ( nRet==-1 )
            break;
        switch( nRet )
        {
        case -2:
            if( aQ.isEmpty() )
                aQ = aReadParam.GetResult();
            break;
        case 'd':
        case 'D':
            if ( aReadParam.GoToTokenParam() )
                aDef = aReadParam.GetResult();
            break;
        }
    }
    if( aDef.isEmpty() )
        aDef = GetFieldResult( pF );

    if ( pF->nId != 0x01 ) // 0x01 fields have no result
    {
        SwInputField aFld( static_cast<SwInputFieldType*>(rDoc.GetSysFldType( RES_INPUTFLD )),
                            aDef, aQ, INP_TXT, 0, false );
        rDoc.getIDocumentContentOperations().InsertPoolItem( *pPaM, SwFmtFld( aFld ), 0 );
    }

    return FLD_OK;
}

// GetFieldResult alloziert einen String und liest das Feld-Resultat ein
OUString SwWW8ImplReader::GetFieldResult( WW8FieldDesc* pF )
{
    long nOldPos = pStrm->Tell();

    WW8_CP nStart = pF->nSRes;              // result start
    long nL = pF->nLRes;                    // result length
    if( !nL )
        return OUString();                  // no result

    if( nL > MAX_FIELDLEN )
        nL = MAX_FIELDLEN;                  // MaxLength, by quoting
                                            // max. 4* as big

    OUString sRes;
    nL = pSBase->WW8ReadString( *pStrm, sRes, pPlcxMan->GetCpOfs() + nStart,
                                nL, eStructCharSet );

    pStrm->Seek( nOldPos );

    //replace both CR 0x0D and VT 0x0B with LF 0x0A
    return sRes.replace(0x0D, 0x0A).replace(0x0B, 0x0A);
}

/*
Bookmarks can be set with fields SET and ASK, and they can be referenced with
REF. When set, they behave like variables in writer, otherwise they behave
like normal bookmarks. We can check whether we should use a show variable
instead of a normal bookmark ref by converting to "show variable" at the end
of the document those refs which look for the content of a bookmark but whose
bookmarks were set with SET or ASK. (See SwWW8FltRefStack)

The other piece of the puzzle is that refs that point to the "location" of the
bookmark will in word actually point to the last location where the bookmark
was set with SET or ASK, not the actual bookmark. This is only noticeable when
a document sets the bookmark more than once. This is because word places the
true bookmark at the location of the last set, but the refs will display the
position of the first set before the ref.

So what we will do is

1) keep a list of all bookmarks that were set, any bookmark names mentioned
here that are refed by content will be converted to show variables.

2) create pseudo bookmarks for every position that a bookmark is set with SET
or ASK but has no existing bookmark. We can then keep a map from the original
bookmark name to the new one. As we parse the document new pseudo names will
replace the older ones, so the map always contains the bookmark of the
location that msword itself would use.

3) word's bookmarks are case insensitive, writers are not. So we need to
map case different versions together, regardless of whether they are
variables or not.

4) when a reference is (first) SET or ASK, the bookmark associated with it
is placed around the 0x14 0x15 result part of the field. We will fiddle
the placement to be the writer equivalent of directly before and after
the field, which gives the same effect and meaning, to do so we must
get any bookmarks in the field range, and begin them immediately before
the set/ask field, and end them directly afterwards. MapBookmarkVariables
returns an identifier of the bookmark attribute to close after inserting
the appropriate set/ask field.
*/
long SwWW8ImplReader::MapBookmarkVariables(const WW8FieldDesc* pF,
    OUString &rOrigName, const OUString &rData)
{
    OSL_ENSURE(pPlcxMan,"No pPlcxMan");
    long nNo;
    /*
    If there was no bookmark associated with this set field, then we create a
    pseudo one and insert it in the document.
    */
    sal_uInt16 nIndex;
    pPlcxMan->GetBook()->MapName(rOrigName);
    OUString sName = pPlcxMan->GetBook()->GetBookmark(
        pF->nSCode, pF->nSCode + pF->nLen, nIndex);
    if (!sName.isEmpty())
    {
        pPlcxMan->GetBook()->SetStatus(nIndex, BOOK_IGNORE);
        nNo = nIndex;
    }
    else
    {
        nNo = pReffingStck->aFieldVarNames.size()+1;
        sName = "WWSetBkmk" + OUString::number(nNo);
        nNo += pPlcxMan->GetBook()->GetIMax();
    }
    pReffedStck->NewAttr(*pPaM->GetPoint(),
        SwFltBookmark( BookmarkToWriter(sName), rData, nNo ));
    pReffingStck->aFieldVarNames[rOrigName] = sName;
    return nNo;
}

/*
Word can set a bookmark with set or with ask, such a bookmark is equivalent to
our variables, but until the end of a document we cannot be sure if a bookmark
is a variable or not, at the end we will have a list of reference names which
were set or asked, all bookmarks using the content of those bookmarks are
converted to show variables, those that reference the position of the field
can be left as references, because a bookmark is also inserted at the position
of a set or ask field, either by word, or in some special cases by the import
filter itself.
*/
SwFltStackEntry *SwWW8FltRefStack::RefToVar(const SwField* pFld,
    SwFltStackEntry &rEntry)
{
    SwFltStackEntry *pRet=0;
    if (pFld && RES_GETREFFLD == pFld->Which())
    {
        //Get the name of the ref field, and see if actually a variable
        const OUString sName = pFld->GetPar1();
        ::std::map<OUString, OUString, SwWW8::ltstr>::const_iterator
            aResult = aFieldVarNames.find(sName);

        if (aResult != aFieldVarNames.end())
        {
            SwGetExpField aFld( (SwGetExpFieldType*)
                pDoc->GetSysFldType(RES_GETEXPFLD), sName, nsSwGetSetExpType::GSE_STRING, 0);
            delete rEntry.pAttr;
            SwFmtFld aTmp(aFld);
            rEntry.pAttr = aTmp.Clone();
            pRet = &rEntry;
        }
    }
    return pRet;
}

OUString SwWW8ImplReader::GetMappedBookmark(const OUString &rOrigName)
{
    OUString sName(BookmarkToWriter(rOrigName));
    OSL_ENSURE(pPlcxMan,"no pPlcxMan");
    pPlcxMan->GetBook()->MapName(sName);

    //See if there has been a variable set with this name, if so get
    //the pseudo bookmark name that was set with it.
    ::std::map<OUString, OUString, SwWW8::ltstr>::const_iterator aResult =
            pReffingStck->aFieldVarNames.find(sName);

    return (aResult == pReffingStck->aFieldVarNames.end())
        ? sName : (*aResult).second;
}

// "ASK"
eF_ResT SwWW8ImplReader::Read_F_InputVar( WW8FieldDesc* pF, OUString& rStr )
{
    OUString sOrigName, aQ;
    OUString aDef;
    WW8ReadFieldParams aReadParam( rStr );
    for (;;)
    {
        const sal_Int32 nRet = aReadParam.SkipToNextToken();
        if ( nRet==-1 )
            break;
        switch( nRet )
        {
        case -2:
            if (sOrigName.isEmpty())
                sOrigName = aReadParam.GetResult();
            else if (aQ.isEmpty())
                aQ = aReadParam.GetResult();
            break;
        case 'd':
        case 'D':
            if ( aReadParam.GoToTokenParam() )
                aDef = aReadParam.GetResult();
            break;
        }
    }

    if (sOrigName.isEmpty())
        return FLD_TAGIGN;  // macht ohne Textmarke keinen Sinn

    const OUString aResult(GetFieldResult(pF));

    //#i24377#, munge Default Text into title as we have only one slot
    //available for aResult and aDef otherwise
    if (!aDef.isEmpty())
    {
        if (!aQ.isEmpty())
            aQ += " - ";
        aQ += aDef;
    }

    const long nNo = MapBookmarkVariables(pF, sOrigName, aResult);

    SwSetExpFieldType* pFT = (SwSetExpFieldType*)rDoc.InsertFldType(
        SwSetExpFieldType(&rDoc, sOrigName, nsSwGetSetExpType::GSE_STRING));
    SwSetExpField aFld(pFT, aResult);
    aFld.SetSubType(nsSwExtendedSubType::SUB_INVISIBLE | nsSwGetSetExpType::GSE_STRING);
    aFld.SetInputFlag(true);
    aFld.SetPromptText( aQ );

    rDoc.getIDocumentContentOperations().InsertPoolItem( *pPaM, SwFmtFld( aFld ), 0 );

    pReffedStck->SetAttr(*pPaM->GetPoint(), RES_FLTR_BOOKMARK, true, nNo);
    return FLD_OK;
}

// "AUTONR"
eF_ResT SwWW8ImplReader::Read_F_ANumber( WW8FieldDesc*, OUString& rStr )
{
    if( !pNumFldType ){     // 1. Mal
        SwSetExpFieldType aT( &rDoc, OUString("AutoNr"), nsSwGetSetExpType::GSE_SEQ );
        pNumFldType = rDoc.InsertFldType( aT );
    }
    SwSetExpField aFld( (SwSetExpFieldType*)pNumFldType, OUString(),
                        GetNumberPara( rStr ) );
    aFld.SetValue( ++nFldNum );
    rDoc.getIDocumentContentOperations().InsertPoolItem( *pPaM, SwFmtFld( aFld ), 0 );
    return FLD_OK;
}

// "SEQ"
eF_ResT SwWW8ImplReader::Read_F_Seq( WW8FieldDesc*, OUString& rStr )
{
    OUString aSequenceName;
    OUString aBook;
    bool bHidden    = false;
    bool bFormat    = false;
    bool bCountOn   = true;
    OUString sStart;
    SvxExtNumType eNumFormat = SVX_NUM_ARABIC;
    WW8ReadFieldParams aReadParam( rStr );
    for (;;)
    {
        const sal_Int32 nRet = aReadParam.SkipToNextToken();
        if ( nRet==-1 )
            break;
        switch( nRet )
        {
        case -2:
            if( aSequenceName.isEmpty() )
                aSequenceName = aReadParam.GetResult();
            else if( aBook.isEmpty() )
                aBook = aReadParam.GetResult();
            break;

        case 'h':
            if( !bFormat )
                bHidden = true;             // Hidden-Flag aktivieren
            break;

        case '*':
            bFormat = true;                 // Format-Flag aktivieren
            if ( aReadParam.SkipToNextToken()!=-2 )
                break;
            if ( aReadParam.GetResult()!="MERGEFORMAT" && aReadParam.GetResult()!="CHARFORMAT" )
                eNumFormat = GetNumTypeFromName( aReadParam.GetResult() );
            break;

        case 'r':
            bCountOn  = false;
            if ( aReadParam.SkipToNextToken()==-2 )
                sStart = aReadParam.GetResult();
            break;

        case 'c':
            bCountOn  = false;
            break;

        case 'n':
            bCountOn  = true;               // Nummer um eins erhoehen (default)
            break;

        case 's':                       // Outline Level
            //#i19682, what am I to do with this value
            break;
        }
    }
    if (aSequenceName.isEmpty() && aBook.isEmpty())
        return FLD_TAGIGN;

    SwSetExpFieldType* pFT = (SwSetExpFieldType*)rDoc.InsertFldType(
                        SwSetExpFieldType( &rDoc, aSequenceName, nsSwGetSetExpType::GSE_SEQ ) );
    SwSetExpField aFld( pFT, OUString(), eNumFormat );

    //#i120654# Add bHidden for /h flag (/h: Hide the field result.)
    if (bHidden)
        aFld.SetSubType(aFld.GetSubType() | nsSwExtendedSubType::SUB_INVISIBLE);

    if (!sStart.isEmpty())
        aFld.SetFormula( ( aSequenceName += "=" ) += sStart );
    else if (!bCountOn)
        aFld.SetFormula(aSequenceName);

    rDoc.getIDocumentContentOperations().InsertPoolItem(*pPaM, SwFmtFld(aFld), 0);
    return FLD_OK;
}

eF_ResT SwWW8ImplReader::Read_F_DocInfo( WW8FieldDesc* pF, OUString& rStr )
{
    sal_uInt16 nSub=0;
    // RegInfoFormat, DefaultFormat fuer DocInfoFelder
    sal_uInt16 nReg = DI_SUB_AUTHOR;
    bool bDateTime = false;

    if( 85 == pF->nId )
    {
        OUString aDocProperty;
        WW8ReadFieldParams aReadParam( rStr );
        for (;;)
        {
            const sal_Int32 nRet = aReadParam.SkipToNextToken();
            if ( nRet==-1 )
                break;
            switch( nRet )
            {
                case -2:
                    if( aDocProperty.isEmpty() )
                        aDocProperty = aReadParam.GetResult();
                    break;
                case '*':
                    //Skip over MERGEFORMAT
                    aReadParam.SkipToNextToken();
                    break;
            }
        }

        aDocProperty = comphelper::string::remove(aDocProperty, '"');

        /*
        There are up to 26 fields that may be meant by 'DocumentProperty'.
        Which of them is to be inserted here ?
        This Problem can only be solved by implementing a name matching
        method that compares the given Parameter String with the four
        possible name sets (english, german, french, spanish)
        */

        static const sal_Char* aName10 = "\x0F"; // SW field code
        static const sal_Char* aName11 // German
            = "TITEL";
        static const sal_Char* aName12 // French
            = "TITRE";
        static const sal_Char* aName13 // English
            = "TITLE";
        static const sal_Char* aName14 // Spanish
            = "TITRO";
        static const sal_Char* aName20 = "\x15"; // SW filed code
        static const sal_Char* aName21 // German
            = "ERSTELLDATUM";
        static const sal_Char* aName22 // French
            = "CR\xC9\xC9";
        static const sal_Char* aName23 // English
            = "CREATED";
        static const sal_Char* aName24 // Spanish
            = "CREADO";
        static const sal_Char* aName30 = "\x16"; // SW filed code
        static const sal_Char* aName31 // German
            = "ZULETZTGESPEICHERTZEIT";
        static const sal_Char* aName32 // French
            = "DERNIERENREGISTREMENT";
        static const sal_Char* aName33 // English
            = "SAVED";
        static const sal_Char* aName34 // Spanish
            = "MODIFICADO";
        static const sal_Char* aName40 = "\x17"; // SW filed code
        static const sal_Char* aName41 // German
            = "ZULETZTGEDRUCKT";
        static const sal_Char* aName42 // French
            = "DERNI\xC8" "REIMPRESSION";
        static const sal_Char* aName43 // English
            = "LASTPRINTED";
        static const sal_Char* aName44 // Spanish
            = "HUPS PUPS";
        static const sal_Char* aName50 = "\x18"; // SW filed code
        static const sal_Char* aName51 // German
            = "\xDC" "BERARBEITUNGSNUMMER";
        static const sal_Char* aName52 // French
            = "NUM\xC9" "RODEREVISION";
        static const sal_Char* aName53 // English
            = "REVISIONNUMBER";
        static const sal_Char* aName54 // Spanish
            = "SNUBBEL BUBBEL";
        static const sal_uInt16 nFldCnt  = 5;

        // additional fields are to be coded soon!   :-)

        static const sal_uInt16 nLangCnt = 4;
        static const sal_Char *aNameSet_26[nFldCnt][nLangCnt+1] =
        {
            {aName10, aName11, aName12, aName13, aName14},
            {aName20, aName21, aName22, aName23, aName24},
            {aName30, aName31, aName32, aName33, aName34},
            {aName40, aName41, aName42, aName43, aName44},
            {aName50, aName51, aName52, aName53, aName54}
        };

        bool bFldFound= false;
        sal_uInt16 nFIdx;
        for(sal_uInt16 nLIdx=1; !bFldFound && (nLangCnt > nLIdx); ++nLIdx)
        {
            for(nFIdx = 0;  !bFldFound && (nFldCnt  > nFIdx); ++nFIdx)
            {
                if( aDocProperty == OUString( aNameSet_26[nFIdx][nLIdx], strlen(aNameSet_26[nFIdx][nLIdx]),
                                              RTL_TEXTENCODING_MS_1252 ) )
                {
                    bFldFound = true;
                    pF->nId   = aNameSet_26[nFIdx][0][0];
                }
            }
        }

        if( !bFldFound )
        {
            SwDocInfoField aFld( (SwDocInfoFieldType*)
                rDoc.GetSysFldType( RES_DOCINFOFLD ), DI_CUSTOM|nReg, aDocProperty, GetFieldResult( pF ) );
            rDoc.getIDocumentContentOperations().InsertPoolItem(*pPaM, SwFmtFld(aFld), 0);

            return FLD_OK;
        }
    }

    switch( pF->nId )
    {
        case 14:
            /* kann alle INFO-Vars!! */
            nSub = DI_KEYS;
            break;
        case 15:
            nSub = DI_TITEL;
            break;
        case 16:
            nSub = DI_THEMA;
            break;
        case 18:
            nSub = DI_KEYS;
            break;
        case 19:
            nSub = DI_COMMENT;
            break;
        case 20:
            nSub = DI_CHANGE;
            nReg = DI_SUB_AUTHOR;
            break;
        case 21:
            nSub = DI_CREATE;
            nReg = DI_SUB_DATE;
            bDateTime = true;
            break;
        case 23:
            nSub = DI_PRINT;
            nReg = DI_SUB_DATE;
            bDateTime = true;
            break;
        case 24:
            nSub = DI_DOCNO;
            break;
        case 22:
            nSub = DI_CHANGE;
            nReg = DI_SUB_DATE;
            bDateTime = true;
            break;
        case 25:
            nSub = DI_CHANGE;
            nReg = DI_SUB_TIME;
            bDateTime = true;
            break;
        case 64: // DOCVARIABLE
            nSub = DI_CUSTOM;
            break;
    }

    sal_uInt32 nFormat = 0;

    sal_uInt16 nLang(0);
    if (bDateTime)
    {
        short nDT = GetTimeDatePara(rStr, nFormat, nLang, pF->nId);
        switch (nDT)
        {
            case NUMBERFORMAT_DATE:
                nReg = DI_SUB_DATE;
                break;
            case NUMBERFORMAT_TIME:
                nReg = DI_SUB_TIME;
                break;
            case NUMBERFORMAT_DATETIME:
                nReg = DI_SUB_DATE;
                break;
            default:
                nReg = DI_SUB_DATE;
                break;
        }
    }

    OUString aData;
    // Extract DOCVARIABLE varname
    if ( 64 == pF->nId )
    {
        WW8ReadFieldParams aReadParam( rStr );
        for (;;)
        {
            const sal_Int32 nRet = aReadParam.SkipToNextToken();
            if ( nRet==-1)
                break;
            switch( nRet )
            {
                case -2:
                    if( aData.isEmpty() )
                        aData = aReadParam.GetResult();
                    break;
                case '*':
                    //Skip over MERGEFORMAT
                    aReadParam.SkipToNextToken();
                    break;
            }
        }

        aData = comphelper::string::remove(aData, '"');
    }

    SwDocInfoField aFld( (SwDocInfoFieldType*)
        rDoc.GetSysFldType( RES_DOCINFOFLD ), nSub|nReg, aData, nFormat );
    if (bDateTime)
        ForceFieldLanguage(aFld, nLang);
    rDoc.getIDocumentContentOperations().InsertPoolItem(*pPaM, SwFmtFld(aFld), 0);

    return FLD_OK;
}

eF_ResT SwWW8ImplReader::Read_F_Author( WW8FieldDesc*, OUString& )
{
        // SH: Das SwAuthorField bezeichnet nicht den urspruenglichen
        // Autor, sondern den aktuellen Benutzer, also besser ueber DocInfo
    SwDocInfoField aFld( (SwDocInfoFieldType*)
                     rDoc.GetSysFldType( RES_DOCINFOFLD ),
                     DI_CREATE|DI_SUB_AUTHOR, OUString() );
    rDoc.getIDocumentContentOperations().InsertPoolItem( *pPaM, SwFmtFld( aFld ), 0 );
    return FLD_OK;
}

eF_ResT SwWW8ImplReader::Read_F_TemplName( WW8FieldDesc*, OUString& )
{
    SwTemplNameField aFld( (SwTemplNameFieldType*)
                     rDoc.GetSysFldType( RES_TEMPLNAMEFLD ), FF_NAME );
    rDoc.getIDocumentContentOperations().InsertPoolItem( *pPaM, SwFmtFld( aFld ), 0 );
    return FLD_OK;
}

// Sowohl das Datum- wie auch das Uhrzeit-Feld kann fuer Datum, fuer Uhrzeit
// oder fuer beides benutzt werden.
eF_ResT SwWW8ImplReader::Read_F_DateTime( WW8FieldDesc*pF, OUString& rStr )
{
    bool bHijri = false;
    WW8ReadFieldParams aReadParam(rStr);
    for (;;)
    {
        const sal_Int32 nTok = aReadParam.SkipToNextToken();
        if ( nTok==-1 )
            break;
        switch (nTok)
        {
            default:
            case 'l':
            case -2:
                break;
            case 'h':
                bHijri = true;
                break;
            case 's':
                //Saka Calendar, should we do something with this ?
                break;
        }
    }

    sal_uInt32 nFormat = 0;

    sal_uInt16 nLang(0);
    short nDT = GetTimeDatePara(rStr, nFormat, nLang, ww::eDATE, bHijri);

    if( NUMBERFORMAT_UNDEFINED == nDT )             // no D/T-Formatstring
    {
        if (32 == pF->nId)
        {
            nDT     = NUMBERFORMAT_TIME;
            nFormat = rDoc.GetNumberFormatter()->GetFormatIndex(
                        NF_TIME_START, LANGUAGE_SYSTEM );
        }
        else
        {
            nDT     = NUMBERFORMAT_DATE;
            nFormat = rDoc.GetNumberFormatter()->GetFormatIndex(
                        NF_DATE_START, LANGUAGE_SYSTEM );
        }
    }

    if (nDT & NUMBERFORMAT_DATE)
    {
        SwDateTimeField aFld((SwDateTimeFieldType*)
            rDoc.GetSysFldType(RES_DATETIMEFLD ), DATEFLD, nFormat);
        ForceFieldLanguage(aFld, nLang);
        rDoc.getIDocumentContentOperations().InsertPoolItem( *pPaM, SwFmtFld( aFld ), 0 );
    }
    else if (nDT == NUMBERFORMAT_TIME)
    {
        SwDateTimeField aFld((SwDateTimeFieldType*)
            rDoc.GetSysFldType(RES_DATETIMEFLD), TIMEFLD, nFormat);
        ForceFieldLanguage(aFld, nLang);
        rDoc.getIDocumentContentOperations().InsertPoolItem( *pPaM, SwFmtFld( aFld ), 0 );
    }

    return FLD_OK;
}

eF_ResT SwWW8ImplReader::Read_F_FileName(WW8FieldDesc*, OUString &rStr)
{
    SwFileNameFormat eType = FF_NAME;
    WW8ReadFieldParams aReadParam(rStr);
    for (;;)
    {
        const sal_Int32 nRet = aReadParam.SkipToNextToken();
        if ( nRet==-1 )
            break;
        switch (nRet)
        {
            case 'p':
                eType = FF_PATHNAME;
                break;
            case '*':
                //Skip over MERGEFORMAT
                aReadParam.SkipToNextToken();
                break;
            default:
                OSL_ENSURE(!this, "unknown option in FileName field");
                break;
        }
    }

    SwFileNameField aFld(
        (SwFileNameFieldType*)rDoc.GetSysFldType(RES_FILENAMEFLD), eType);
    rDoc.getIDocumentContentOperations().InsertPoolItem(*pPaM, SwFmtFld(aFld), 0);
    return FLD_OK;
}

eF_ResT SwWW8ImplReader::Read_F_Anz( WW8FieldDesc* pF, OUString& rStr )
{
    sal_uInt16 nSub = DS_PAGE;                  // page number
    switch ( pF->nId ){
        case 27: nSub = DS_WORD; break;         // number of words
        case 28: nSub = DS_CHAR; break;         // number of characters
    }
    SwDocStatField aFld( (SwDocStatFieldType*)
                         rDoc.GetSysFldType( RES_DOCSTATFLD ), nSub,
                         GetNumberPara( rStr ) );
    rDoc.getIDocumentContentOperations().InsertPoolItem( *pPaM, SwFmtFld( aFld ), 0 );
    return FLD_OK;
}

eF_ResT SwWW8ImplReader::Read_F_CurPage( WW8FieldDesc*, OUString& rStr )
{
    // page number
    SwPageNumberField aFld( (SwPageNumberFieldType*)
        rDoc.GetSysFldType( RES_PAGENUMBERFLD ), PG_RANDOM,
        GetNumberPara(rStr, true));

    rDoc.getIDocumentContentOperations().InsertPoolItem( *pPaM, SwFmtFld( aFld ), 0 );
    return FLD_OK;
}

eF_ResT SwWW8ImplReader::Read_F_Symbol( WW8FieldDesc*, OUString& rStr )
{
    //e.g. #i20118#
    OUString aQ;
    OUString aName;
    sal_Int32 nSize = 0;
    WW8ReadFieldParams aReadParam( rStr );
    for (;;)
    {
        const sal_Int32 nRet = aReadParam.SkipToNextToken();
        if ( nRet==-1 )
            break;
        switch( nRet )
        {
        case -2:
            if( aQ.isEmpty() )
                aQ = aReadParam.GetResult();
            break;
        case 'f':
        case 'F':
            if ( aReadParam.GoToTokenParam() )
                aName = aReadParam.GetResult();
            break;
        case 's':
        case 'S':
            if ( aReadParam.GoToTokenParam() )
            {
                const OUString aSiz = aReadParam.GetResult();
                if ( !aSiz.isEmpty() )
                    nSize = aSiz.toInt32() * 20; // pT -> twip
            }
            break;
        }
    }
    if( aQ.isEmpty() )
        return FLD_TAGIGN;                      // -> no 0-char in text

    if (sal_Unicode cChar = static_cast<sal_Unicode>(aQ.toInt32()))
    {
        if (!aName.isEmpty())                           // Font Name set ?
        {
            SvxFontItem aFont(FAMILY_DONTKNOW, aName, OUString(),
                PITCH_DONTKNOW, RTL_TEXTENCODING_SYMBOL, RES_CHRATR_FONT);
            NewAttr(aFont);                       // new Font
        }

        if (nSize > 0)  //#i20118#
        {
            SvxFontHeightItem aSz(nSize, 100, RES_CHRATR_FONTSIZE);
            NewAttr(aSz);
        }

        rDoc.getIDocumentContentOperations().InsertString(*pPaM, OUString(cChar));

        if (nSize > 0)
            pCtrlStck->SetAttr(*pPaM->GetPoint(), RES_CHRATR_FONTSIZE);
        if (!aName.isEmpty())
            pCtrlStck->SetAttr(*pPaM->GetPoint(), RES_CHRATR_FONT);
    }
    else
    {
        rDoc.getIDocumentContentOperations().InsertString(*pPaM, OUString("###"));
    }

    return FLD_OK;
}

// "EINBETTEN"
eF_ResT SwWW8ImplReader::Read_F_Embedd( WW8FieldDesc*, OUString& rStr )
{
    OUString sHost;

    WW8ReadFieldParams aReadParam( rStr );
    for (;;)
    {
        const sal_Int32 nRet = aReadParam.SkipToNextToken();
        if ( nRet==-1 )
            break;
        switch( nRet )
        {
        case -2:
            sHost = aReadParam.GetResult();
            break;

        case 's':
            // use ObjectSize
            break;
        }
    }

    if( bObj && nPicLocFc )
        nObjLocFc = nPicLocFc;
    bEmbeddObj = true;
    return FLD_TEXT;
}

// "SET"
eF_ResT SwWW8ImplReader::Read_F_Set( WW8FieldDesc* pF, OUString& rStr )
{
    OUString sOrigName;
    OUString sVal;
    WW8ReadFieldParams aReadParam( rStr );
    for (;;)
    {
        const sal_Int32 nRet = aReadParam.SkipToNextToken();
        if ( nRet==-1 )
            break;
        switch( nRet )
        {
        case -2:
            if (sOrigName.isEmpty())
                sOrigName = aReadParam.GetResult();
            else if (sVal.isEmpty())
                sVal = aReadParam.GetResult();
            break;
        }
    }

    const long nNo = MapBookmarkVariables(pF, sOrigName, sVal);

    SwFieldType* pFT = rDoc.InsertFldType( SwSetExpFieldType( &rDoc, sOrigName,
        nsSwGetSetExpType::GSE_STRING ) );
    SwSetExpField aFld( (SwSetExpFieldType*)pFT, sVal, ULONG_MAX );
    aFld.SetSubType(nsSwExtendedSubType::SUB_INVISIBLE | nsSwGetSetExpType::GSE_STRING);

    rDoc.getIDocumentContentOperations().InsertPoolItem( *pPaM, SwFmtFld( aFld ), 0 );

    pReffedStck->SetAttr(*pPaM->GetPoint(), RES_FLTR_BOOKMARK, true, nNo);

    return FLD_OK;
}

// "REF"
eF_ResT SwWW8ImplReader::Read_F_Ref( WW8FieldDesc*, OUString& rStr )
{                                                       // Reference - Field
    OUString sOrigBkmName;
    REFERENCEMARK eFormat = REF_CONTENT;

    WW8ReadFieldParams aReadParam( rStr );
    for (;;)
    {
        const sal_Int32 nRet = aReadParam.SkipToNextToken();
        if ( nRet==-1 )
            break;
        switch( nRet )
        {
        case -2:
            if( sOrigBkmName.isEmpty() ) // get name of bookmark
                sOrigBkmName = aReadParam.GetResult();
            break;

        /* References to numbers in Word could be either to a numbered
        paragraph or to a chapter number. However Word does not seem to
        have the capability we do, of refering to the chapter number some
        other bookmark is in. As a result, cross-references to chapter
        numbers in a word document will be cross-references to a numbered
        paragraph, being the chapter heading paragraph. As it happens, our
        cross-references to numbered paragraphs will do the right thing
        when the target is a numbered chapter heading, so there is no need
        for us to use the REF_CHAPTER bookmark format on import.
        */
        case 'n':
            eFormat = REF_NUMBER_NO_CONTEXT;
            break;
        case 'r':
            eFormat = REF_NUMBER;
            break;
        case 'w':
            eFormat = REF_NUMBER_FULL_CONTEXT;
            break;

        case 'p':
            eFormat = REF_UPDOWN;
            break;
        case 'h':
            break;
        default:
            // unimplemented switch: just do 'nix nought nothing'  :-)
            break;
        }
    }

    OUString sBkmName(GetMappedBookmark(sOrigBkmName));

    SwGetRefField aFld(
        (SwGetRefFieldType*)rDoc.GetSysFldType( RES_GETREFFLD ),
        sBkmName,REF_BOOKMARK,0,eFormat);

    if (eFormat == REF_CONTENT)
    {
        /*
        If we are just inserting the contents of the bookmark, then it
        is possible that the bookmark is actually a variable, so we
        must store it until the end of the document to see if it was,
        in which case we'll turn it into a show variable
        */
        pReffingStck->NewAttr( *pPaM->GetPoint(), SwFmtFld(aFld) );
        pReffingStck->SetAttr( *pPaM->GetPoint(), RES_TXTATR_FIELD);
    }
    else
    {
        rDoc.getIDocumentContentOperations().InsertPoolItem(*pPaM, SwFmtFld(aFld), 0);
    }
    return FLD_OK;
}

// Note Reference - Field
eF_ResT SwWW8ImplReader::Read_F_NoteReference( WW8FieldDesc*, OUString& rStr )
{
    OUString aBkmName;
    bool bAboveBelow = false;

    WW8ReadFieldParams aReadParam( rStr );
    for (;;)
    {
        const sal_Int32 nRet = aReadParam.SkipToNextToken();
        if ( nRet==-1 )
            break;
        switch( nRet )
        {
        case -2:
            if( aBkmName.isEmpty() ) // get name of foot/endnote
                aBkmName = aReadParam.GetResult();
            break;
        case 'r':
            // activate flag 'Chapter Number'
            break;
        case 'p':
            bAboveBelow = true;
            break;
        case 'h':
            break;
        default:
            // unimplemented switch: just do 'nix nought nothing'  :-)
            break;
        }
    }

    // set Sequence No of corresponding Foot-/Endnote to Zero
    // (will be corrected in
    SwGetRefField aFld( (SwGetRefFieldType*)
        rDoc.GetSysFldType( RES_GETREFFLD ), aBkmName, REF_FOOTNOTE, 0,
        REF_ONLYNUMBER );
    pReffingStck->NewAttr(*pPaM->GetPoint(), SwFmtFld(aFld));
    pReffingStck->SetAttr(*pPaM->GetPoint(), RES_TXTATR_FIELD);
    if (bAboveBelow)
    {
        SwGetRefField aFld2( (SwGetRefFieldType*)
            rDoc.GetSysFldType( RES_GETREFFLD ),aBkmName, REF_FOOTNOTE, 0,
            REF_UPDOWN );
        pReffingStck->NewAttr(*pPaM->GetPoint(), SwFmtFld(aFld2));
        pReffingStck->SetAttr(*pPaM->GetPoint(), RES_TXTATR_FIELD);
    }
    return FLD_OK;
}

// "SEITENREF"
eF_ResT SwWW8ImplReader::Read_F_PgRef( WW8FieldDesc*, OUString& rStr )
{
    OUString sOrigName;
    WW8ReadFieldParams aReadParam( rStr );
    for (;;)
    {
        const sal_Int32 nRet = aReadParam.SkipToNextToken();
        if ( nRet==-1 )
            break;
        else if ( nRet == -2 && sOrigName.isEmpty() )
        {
            sOrigName = aReadParam.GetResult();
        }
    }

    const OUString sName(GetMappedBookmark(sOrigName));

    // loading page reference field in TOX
    if (mbLoadingTOXCache)
    {
        // insert page ref representation as plain text --> return FLD_TEXT
        // if there is no hyperlink settings for current toc and referenced bookmark is available,
        // assign link to current ref area
        if (!mbLoadingTOXHyperlink && !sName.isEmpty())
        {
            // #i120879# add cross reference bookmark name prefix, if it
            // matches internal TOC bookmark naming convention
            OUString sBookmarkName;
            if ( IsTOCBookmarkName( sName ) )
            {
                sBookmarkName = IDocumentMarkAccess::GetCrossRefHeadingBookmarkNamePrefix();
                sBookmarkName += sName;
                // track <sBookmarkName> as referenced TOC bookmark.
                pReffedStck->aReferencedTOCBookmarks.insert( sBookmarkName );
            }
            else
            {
                sBookmarkName = sName;
            }
            OUString sURL = "#" + sBookmarkName;
            const OUString sTarget;
            SwFmtINetFmt aURL( sURL, sTarget );
            const OUString sLinkStyle("Index Link");
            const sal_uInt16 nPoolId =
                SwStyleNameMapper::GetPoolIdFromUIName( sLinkStyle, nsSwGetPoolIdFromName::GET_POOLID_CHRFMT );
            aURL.SetVisitedFmtAndId( sLinkStyle, nPoolId);
            aURL.SetINetFmtAndId( sLinkStyle, nPoolId );
            pCtrlStck->NewAttr( *pPaM->GetPoint(), aURL );
        }
        return FLD_TEXT;
    }

    // #i120879# add cross reference bookmark name prefix, if it matches
    // internal TOC bookmark naming convention
    OUString sPageRefBookmarkName;
    if ( IsTOCBookmarkName( sName ) )
    {
        sPageRefBookmarkName = IDocumentMarkAccess::GetCrossRefHeadingBookmarkNamePrefix();
        sPageRefBookmarkName += sName;
        // track <sPageRefBookmarkName> as referenced TOC bookmark.
        pReffedStck->aReferencedTOCBookmarks.insert( sPageRefBookmarkName );
    }
    else
    {
        sPageRefBookmarkName = sName;
    }
    SwGetRefField aFld( (SwGetRefFieldType*)rDoc.GetSysFldType( RES_GETREFFLD ),
                        sPageRefBookmarkName, REF_BOOKMARK, 0, REF_PAGE );
    rDoc.getIDocumentContentOperations().InsertPoolItem( *pPaM, SwFmtFld( aFld ), 0 );

    return FLD_OK;
}

//helper function
//For MS MacroButton field, the symbol in plain text is always "(" (0x28),
//which should be mapped according to the macro type
bool ConvertMacroSymbol( const OUString& rName, OUString& rReference )
{
    bool bConverted = false;
    if( rReference == "(" )
    {
        bConverted = true;
        sal_Unicode cSymbol = sal_Unicode(); // silence false warning
        if (rName == "CheckIt")
            cSymbol = 0xF06F;
        else if (rName == "UncheckIt")
            cSymbol = 0xF0FE;
        else if (rName == "ShowExample")
            cSymbol = 0xF02A;
        //else if... : todo
        else
            bConverted = false;

        if( bConverted )
            rReference = OUString(cSymbol);
    }
    return bConverted;
}

// "MACROSCHALTFL"ACHE"
eF_ResT SwWW8ImplReader::Read_F_Macro( WW8FieldDesc*, OUString& rStr)
{
    OUString aName;
    OUString aVText;
    bool bNewVText = true;
    bool bBracket  = false;
    WW8ReadFieldParams aReadParam( rStr );

    sal_Int32 nOffset = 0;

    for (;;)
    {
        const sal_Int32 nRet = aReadParam.SkipToNextToken();
        if ( nRet==-1 )
            break;
        switch( nRet )
        {
        case -2:
            if( aName.isEmpty() )
                aName = aReadParam.GetResult();
            else if( aVText.isEmpty() || bBracket )
            {
                nOffset = aReadParam.GetTokenSttPtr() + 1;

                if( bBracket )
                    aVText += " ";
                aVText += aReadParam.GetResult();
                if (bNewVText)
                {
                    bBracket = (aVText[0] == '[');
                    bNewVText = false;
                }
                else if( aVText.endsWith("]") )
                    bBracket  = false;
            }
            break;
        }
    }
    if( aName.isEmpty() )
        return FLD_TAGIGN;  // makes no sense without Makro-Name

    //try converting macro symbol according to macro name
    bool bApplyWingdings = ConvertMacroSymbol( aName, aVText );
    aName = "StarOffice.Standard.Modul1." + aName;

    SwMacroField aFld( (SwMacroFieldType*)
                    rDoc.GetSysFldType( RES_MACROFLD ), aName, aVText );

    if( !bApplyWingdings )
    {

        rDoc.getIDocumentContentOperations().InsertPoolItem( *pPaM, SwFmtFld( aFld ), 0 );
        WW8_CP nOldCp = pPlcxMan->Where();
        WW8_CP nCp = nOldCp + nOffset;

        SwPaM aPaM(*pPaM);
        aPaM.SetMark();
        aPaM.Move(fnMoveBackward);
        aPaM.Exchange();

        mpPostProcessAttrsInfo = new WW8PostProcessAttrsInfo(nCp, nCp, aPaM);
    }
    else
    {
        //set Wingdings font
        sal_uInt16 i = 0;
        for ( ; i < pFonts->GetMax(); i++ )
        {
            FontFamily eFamily;
            OUString aFontName;
            FontPitch ePitch;
            rtl_TextEncoding eSrcCharSet;
            if( GetFontParams( i, eFamily, aFontName, ePitch, eSrcCharSet )
                && aFontName=="Wingdings" )
            {
                break;
            }
        }

        if ( i < pFonts->GetMax() )
        {

            SetNewFontAttr( i, true, RES_CHRATR_FONT );
            rDoc.getIDocumentContentOperations().InsertPoolItem( *pPaM, SwFmtFld( aFld ), 0 );
            pCtrlStck->SetAttr( *pPaM->GetPoint(), RES_CHRATR_FONT );
            ResetCharSetVars();
        }
    }

    return FLD_OK;
}

WW8PostProcessAttrsInfo::WW8PostProcessAttrsInfo(WW8_CP nCpStart, WW8_CP nCpEnd,
                                                 SwPaM & rPaM)
: mbCopy(false),
  mnCpStart(nCpStart),
  mnCpEnd(nCpEnd),
  mPaM(*rPaM.GetPoint(), *rPaM.GetMark()),
  mItemSet(rPaM.GetDoc()->GetAttrPool(), RES_CHRATR_BEGIN, RES_PARATR_END - 1)
{
}

bool CanUseRemoteLink(const OUString &rGrfName)
{
    bool bUseRemote = false;
    try
    {
        ::ucbhelper::Content aCnt(rGrfName,
            uno::Reference< ucb::XCommandEnvironment >(),
            comphelper::getProcessComponentContext() );
        OUString   aTitle;

        aCnt.getPropertyValue("Title") >>= aTitle;
        bUseRemote = !aTitle.isEmpty();
    }
    catch ( ... )
    {
        // this file did not exist, so we will not set this as graphiclink
        bUseRemote = false;
    }
    return bUseRemote;
}

// "EINF"UGENGRAFIK"
eF_ResT SwWW8ImplReader::Read_F_IncludePicture( WW8FieldDesc*, OUString& rStr )
{
    OUString aGrfName;
    bool bEmbedded = true;

    WW8ReadFieldParams aReadParam( rStr );
    for (;;)
    {
        const sal_Int32 nRet = aReadParam.SkipToNextToken();
        if ( nRet==-1 )
            break;
        switch( nRet )
        {
        case -2:
            if (aGrfName.isEmpty())
                aGrfName = ConvertFFileName(aReadParam.GetResult());
            break;

        case 'd':
            bEmbedded = false;
            break;

        case 'c':// den Converter-Namen ueberlesen
            aReadParam.FindNextStringPiece();
            break;
        }
    }

    if (!bEmbedded)
        bEmbedded = !CanUseRemoteLink(aGrfName);

    if (!bEmbedded)
    {
        /*
            Besonderheit:

            Wir setzen jetzt den Link ins Doc und merken uns den SwFlyFrmFmt.
            Da wir ja unten auf jjeden Fall mit Return-Wert FLD_READ_FSPA enden,
            wird der Skip-Wert so bemessen, dass das folgende Char-1 eingelesen
            wird.
            Wenn wir dann in SwWW8ImplReader::ImportGraf() reinlaufen, wird
            erkannt, dass wir soeben einen Grafik-Link inserted haben und
            das passende SwAttrSet wird ins Frame-Format eingesetzt.
        */
        SfxItemSet aFlySet( rDoc.GetAttrPool(), RES_FRMATR_BEGIN,
            RES_FRMATR_END-1 );
        aFlySet.Put( SwFmtAnchor( FLY_AS_CHAR ) );
        aFlySet.Put( SwFmtVertOrient( 0, text::VertOrientation::TOP, text::RelOrientation::FRAME ));
        pFlyFmtOfJustInsertedGraphic = rDoc.getIDocumentContentOperations().Insert( *pPaM,
                                                    aGrfName,
                                                    OUString(),
                                                    0,          // Graphic*
                                                    &aFlySet,
                                                    0, 0);         // SwFrmFmt*
        maGrfNameGenerator.SetUniqueGraphName(pFlyFmtOfJustInsertedGraphic,
            INetURLObject(aGrfName).GetBase());
    }
    return FLD_READ_FSPA;
}

OUString wwSectionNamer::UniqueName()
{
    const OUString aName(msFileLinkSeed + OUString::number(++mnFileSectionNo));
    return mrDoc.GetUniqueSectionName(&aName);
}

// "EINFUEGENTEXT"
eF_ResT SwWW8ImplReader::Read_F_IncludeText( WW8FieldDesc* /*pF*/, OUString& rStr )
{
    OUString aPara;
    OUString aBook;
    WW8ReadFieldParams aReadParam( rStr );
    for (;;)
    {
        const sal_Int32 nRet = aReadParam.SkipToNextToken();
        if ( nRet==-1 )
            break;
        switch( nRet )
        {
            case -2:
                if( aPara.isEmpty() )
                    aPara = aReadParam.GetResult();
                else if( aBook.isEmpty() )
                    aBook = aReadParam.GetResult();
                break;
            case '*':
                //Skip over MERGEFORMAT
                aReadParam.SkipToNextToken();
                break;
        }
    }
    aPara = ConvertFFileName(aPara);

    if (!aBook.isEmpty() && aBook[ 0 ] != '\\')
    {
        // Bereich aus Quelle ( kein Switch ) ?
        ConvertUFName(aBook);
        aPara += OUString(sfx2::cTokenSeparator);
        aPara += OUString(sfx2::cTokenSeparator);
        aPara += aBook;
    }

    /*
    ##509##
    What we will do is insert a section to be linked to a file, but just in
    case the file is not available we will fill in the section with the stored
    content of this winword field as a fallback.
    */
    SwPosition aTmpPos(*pPaM->GetPoint());

    SwSectionData aSection(FILE_LINK_SECTION,
            maSectionNameGenerator.UniqueName());
    aSection.SetLinkFileName( aPara );
    aSection.SetProtectFlag(true);

    SwSection *const pSection =
        rDoc.InsertSwSection(*pPaM, aSection, 0, 0, false);
    OSL_ENSURE(pSection, "no section inserted");
    if (!pSection)
        return FLD_TEXT;
    const SwSectionNode* pSectionNode = pSection->GetFmt()->GetSectionNode();
    OSL_ENSURE(pSectionNode, "no section node!");
    if (!pSectionNode)
        return FLD_TEXT;

    pPaM->GetPoint()->nNode = pSectionNode->GetIndex()+1;
    pPaM->GetPoint()->nContent.Assign(pPaM->GetCntntNode(), 0 );

    //we have inserted a section before this point, so adjust pos
    //for future page/section segment insertion
    maSectionManager.PrependedInlineNode(aTmpPos, pPaM->GetNode());

    return FLD_TEXT;
}

// "SERIENDRUCKFELD"
eF_ResT SwWW8ImplReader::Read_F_DBField( WW8FieldDesc* pF, OUString& rStr )
{
#if !HAVE_FEATURE_DBCONNECTIVITY
    (void) pF;
    (void) rStr;
#else
    OUString aName;
    WW8ReadFieldParams aReadParam( rStr );
    for (;;)
    {
        const sal_Int32 nRet = aReadParam.SkipToNextToken();
        if ( nRet==-1 )
            break;
        switch( nRet )
        {
        case -2:
            if( aName.isEmpty() )
                aName = aReadParam.GetResult();
            break;
        }
    }
    SwDBFieldType aD( &rDoc, aName, SwDBData() );   // Datenbank: Nichts

    SwFieldType* pFT = rDoc.InsertFldType( aD );
    SwDBField aFld( (SwDBFieldType*)pFT );
    aFld.SetFieldCode( rStr );

    OUString aResult;
    pSBase->WW8ReadString( *pStrm, aResult, pPlcxMan->GetCpOfs()+
                           pF->nSRes, pF->nLRes, eTextCharSet );

    aResult = aResult.replace( '\xb', '\n' );

    aFld.InitContent(aResult);

    rDoc.getIDocumentContentOperations().InsertPoolItem(*pPaM, SwFmtFld( aFld ), 0);
#endif
    return FLD_OK;
}

// "N"ACHSTER"
eF_ResT SwWW8ImplReader::Read_F_DBNext( WW8FieldDesc*, OUString& )
{
#if HAVE_FEATURE_DBCONNECTIVITY
    SwDBNextSetFieldType aN;
    SwFieldType* pFT = rDoc.InsertFldType( aN );
    SwDBNextSetField aFld( (SwDBNextSetFieldType*)pFT, OUString(), OUString(),
                            SwDBData() );       // Datenbank: Nichts
    rDoc.getIDocumentContentOperations().InsertPoolItem( *pPaM, SwFmtFld( aFld ), 0 );
#endif
    return FLD_OK;
}

// "DATENSATZ"
eF_ResT SwWW8ImplReader::Read_F_DBNum( WW8FieldDesc*, OUString& )
{
#if HAVE_FEATURE_DBCONNECTIVITY
    SwDBSetNumberFieldType aN;
    SwFieldType* pFT = rDoc.InsertFldType( aN );
    SwDBSetNumberField aFld( (SwDBSetNumberFieldType*)pFT,
                           SwDBData() );            // Datenbank: Nichts
    rDoc.getIDocumentContentOperations().InsertPoolItem( *pPaM, SwFmtFld( aFld ), 0 );
#endif
    return FLD_OK;
}

/*
    EQ , only the usage for
    a. Combined Characters supported, must be exactly in the form that word
    only accepts as combined charactersm, i.e.
    eq \o(\s\up Y(XXX),\s\do Y(XXX))
    b. Ruby Text supported, must be in the form that word recognizes as being
    ruby text
    ...
*/
eF_ResT SwWW8ImplReader::Read_F_Equation( WW8FieldDesc*, OUString& rStr )
{
    WW8ReadFieldParams aReadParam( rStr );
    const sal_Int32 cChar = aReadParam.SkipToNextToken();
    if ('o' == cChar || 'O' == cChar)
    {
        EquationResult aResult(ParseCombinedChars(rStr));

        if (aResult.sType == "Input")
        {
            SwInputField aFld( static_cast<SwInputFieldType*>(rDoc.GetSysFldType( RES_INPUTFLD )),
                aResult.sResult, aResult.sResult, INP_TXT, 0 );
            rDoc.getIDocumentContentOperations().InsertPoolItem( *pPaM, SwFmtFld( aFld ), 0 ); // insert input field
        }
        else if (aResult.sType == "CombinedCharacters")
        {
            SwCombinedCharField aFld((SwCombinedCharFieldType*)
                rDoc.GetSysFldType(RES_COMBINED_CHARS), aResult.sType);
            rDoc.getIDocumentContentOperations().InsertPoolItem(*pPaM, SwFmtFld(aFld), 0);
        }
    }
    else if ('*' == cChar)
        Read_SubF_Ruby(aReadParam);

    return FLD_OK;
}

void SwWW8ImplReader::Read_SubF_Ruby( WW8ReadFieldParams& rReadParam)
{
    sal_uInt16 nJustificationCode=0;
    OUString sFontName;
    sal_uInt32 nFontSize=0;
    OUString sRuby;
    OUString sText;
    for (;;)
    {
        const sal_Int32 nRet = rReadParam.SkipToNextToken();
        if ( nRet==-1 )
            break;
        switch( nRet )
        {
        case -2:
            {
                OUString sTemp = rReadParam.GetResult();
                if( sTemp.startsWithIgnoreAsciiCase( "jc" ) )
                {
                    sTemp = sTemp.copy(2);
                    nJustificationCode = static_cast<sal_uInt16>(sTemp.toInt32());
                }
                else if( sTemp.startsWithIgnoreAsciiCase( "hps" ) )
                {
                    sTemp = sTemp.copy(3);
                    nFontSize= static_cast<sal_uInt32>(sTemp.toInt32());
                }
                else if( sTemp.startsWithIgnoreAsciiCase( "Font:" ) )
                {
                    sTemp = sTemp.copy(5);
                    sFontName = sTemp;
                }
            }
            break;
        case '*':
            break;
        case 'o':
            for (;;)
            {
                const sal_Int32 nRes = rReadParam.SkipToNextToken();
                if ( nRes==-1 )
                    break;
                if ('u' == nRes)
                {
                    if (-2 == rReadParam.SkipToNextToken() &&
                        rReadParam.GetResult().startsWithIgnoreAsciiCase("p"))
                    {
                        if (-2 == rReadParam.SkipToNextToken())
                        {
                            OUString sPart = rReadParam.GetResult();
                            sal_Int32 nBegin = sPart.indexOf('(');

                            //Word disallows brackets in this field,
                            sal_Int32 nEnd = sPart.indexOf(')');

                            if ((nBegin != -1) &&
                                (nEnd != -1) && (nBegin < nEnd))
                            {
                                sRuby = sPart.copy(nBegin+1,nEnd-nBegin-1);
                            }
                            if (-1 != nEnd)
                            {
                                if (-1 ==
                                    (nBegin = sPart.indexOf(',',nEnd)))
                                {
                                    nBegin = sPart.indexOf(';',nEnd);
                                }
                                nEnd = sPart.lastIndexOf(')');
                            }
                            if ((nBegin != -1) && (nEnd != -1) && (nBegin < nEnd))
                            {
                                sText = sPart.copy(nBegin+1,nEnd-nBegin-1);
                            }
                        }
                    }
                }

            }
            break;
        }
    }

    //Translate and apply
    if (!sRuby.isEmpty() && !sText.isEmpty() && !sFontName.isEmpty() && nFontSize)
    {
        switch (nJustificationCode)
        {
            case 0:
                nJustificationCode=1;
                break;
            case 1:
                nJustificationCode=3;
                break;
            case 2:
                nJustificationCode=4;
                break;
            default:
            case 3:
                nJustificationCode=0;
                break;
            case 4:
                nJustificationCode=2;
                break;
        }

        SwFmtRuby aRuby(sRuby);
        const SwCharFmt *pCharFmt=0;
        //Make a guess at which of asian of western we should be setting
        sal_uInt16 nScript;
        if (g_pBreakIt->GetBreakIter().is())
            nScript = g_pBreakIt->GetBreakIter()->getScriptType(sRuby, 0);
        else
            nScript = i18n::ScriptType::ASIAN;

        //Check to see if we already have a ruby charstyle that this fits
        std::vector<const SwCharFmt*>::const_iterator aEnd =
            aRubyCharFmts.end();
        for(std::vector<const SwCharFmt*>::const_iterator aIter
            = aRubyCharFmts.begin(); aIter != aEnd; ++aIter)
        {
            const SvxFontHeightItem &rFH =
                ItemGet<SvxFontHeightItem>(*(*aIter),
                GetWhichOfScript(RES_CHRATR_FONTSIZE,nScript));
            if (rFH.GetHeight() == nFontSize*10)
            {
                const SvxFontItem &rF = ItemGet<SvxFontItem>(*(*aIter),
                    GetWhichOfScript(RES_CHRATR_FONT,nScript));
                if (rF.GetFamilyName().equals(sFontName))
                {
                    pCharFmt=*aIter;
                    break;
                }
            }
        }

        //Create a new char style if necessary
        if (!pCharFmt)
        {
            SwCharFmt *pFmt=0;
            OUString aNm;
            //Take this as the base name
            SwStyleNameMapper::FillUIName(RES_POOLCHR_RUBYTEXT,aNm);
            aNm+=OUString::number(aRubyCharFmts.size()+1);
            pFmt = rDoc.MakeCharFmt(aNm,(SwCharFmt*)rDoc.GetDfltCharFmt());
            SvxFontHeightItem aHeightItem(nFontSize*10, 100, RES_CHRATR_FONTSIZE);
            SvxFontItem aFontItem(FAMILY_DONTKNOW,sFontName,
                OUString(), PITCH_DONTKNOW, RTL_TEXTENCODING_DONTKNOW, RES_CHRATR_FONT);
            aHeightItem.SetWhich(GetWhichOfScript(RES_CHRATR_FONTSIZE,nScript));
            aFontItem.SetWhich(GetWhichOfScript(RES_CHRATR_FONT,nScript));
            pFmt->SetFmtAttr(aHeightItem);
            pFmt->SetFmtAttr(aFontItem);
            aRubyCharFmts.push_back(pFmt);
            pCharFmt = pFmt;
        }

        //Set the charstyle and justification
        aRuby.SetCharFmtName(pCharFmt->GetName());
        aRuby.SetCharFmtId(pCharFmt->GetPoolFmtId());
        aRuby.SetAdjustment(nJustificationCode);

        NewAttr(aRuby);
        rDoc.getIDocumentContentOperations().InsertString( *pPaM, sText );
        pCtrlStck->SetAttr( *pPaM->GetPoint(), RES_TXTATR_CJK_RUBY );
    }
}

//        "table of ..." fields

static void lcl_toxMatchACSwitch(  SwWW8ImplReader& /*rReader*/,
                            SwDoc& rDoc,
                            SwTOXBase& rBase,
                            WW8ReadFieldParams& rParam,
                            SwCaptionDisplay eCaptionType)
{
    if ( rParam.GoToTokenParam() )
    {
        SwTOXType* pType = (SwTOXType*)rDoc.GetTOXType( TOX_ILLUSTRATIONS, 0);
        rBase.RegisterToTOXType( *pType );
        rBase.SetCaptionDisplay( eCaptionType );
        // Read Sequence Name and store in TOXBase
        OUString sSeqName( rParam.GetResult() );
        lcl_ConvertSequenceName( sSeqName );
        rBase.SetSequenceName( sSeqName );
    }
}

static void EnsureMaxLevelForTemplates(SwTOXBase& rBase)
{
    //If the TOC contains Template entries at levels > the evaluation level
    //that was initially taken from the max normal outline level of the word TOC
    //then we cannot use that for the evaluation level because writer cuts off
    //all styles above that level, while word just cuts off the "standard"
    //outline styles, we have no option but to expand to the highest level
    //Word included.
    if ((rBase.GetLevel() != MAXLEVEL) && (nsSwTOXElement::TOX_TEMPLATE & rBase.GetCreateType()))
    {
        for (sal_uInt16 nI = MAXLEVEL; nI > 0; --nI)
        {
            if (!rBase.GetStyleNames(nI-1).isEmpty())
            {
                rBase.SetLevel(nI);
                break;
            }
        }
    }
}

static void lcl_toxMatchTSwitch(SwWW8ImplReader& rReader, SwTOXBase& rBase,
    WW8ReadFieldParams& rParam)
{
    if ( rParam.GoToTokenParam() )
    {
        OUString sParams( rParam.GetResult() );
        if( !sParams.isEmpty() )
        {
            sal_Int32 nIndex = 0;

            // Delimiters between styles and style levels appears to allow both ; and ,

            OUString sTemplate( sParams.getToken(0, ';', nIndex) );
            if( -1 == nIndex )
            {
                nIndex=0;
                sTemplate = sParams.getToken(0, ',', nIndex);
            }
            if( -1 == nIndex )
            {
                const SwFmt* pStyle = rReader.GetStyleWithOrgWWName(sTemplate);
                if( pStyle )
                    sTemplate = pStyle->GetName();
                // Store Style for Level 0 into TOXBase
                rBase.SetStyleNames( sTemplate, 0 );
            }
            else while( -1 != nIndex )
            {
                sal_Int32 nOldIndex=nIndex;
                sal_uInt16 nLevel = static_cast<sal_uInt16>(
                    sParams.getToken(0, ';', nIndex).toInt32());
                if( -1 == nIndex )
                {
                    nIndex = nOldIndex;
                    nLevel = static_cast<sal_uInt16>(
                        sParams.getToken(0, ',', nIndex).toInt32());
                }

                if( (0 < nLevel) && (MAXLEVEL >= nLevel) )
                {
                    nLevel--;
                    // Store Style and Level into TOXBase
                    const SwFmt* pStyle
                            = rReader.GetStyleWithOrgWWName( sTemplate );

                    if( pStyle )
                        sTemplate = pStyle->GetName();

                    OUString sStyles( rBase.GetStyleNames( nLevel ) );
                    if( !sStyles.isEmpty() )
                        sStyles += OUString(TOX_STYLE_DELIMITER);
                    sStyles += sTemplate;
                    rBase.SetStyleNames( sStyles, nLevel );
                }
                // read next style name...
                nOldIndex = nIndex;
                sTemplate = sParams.getToken(0, ';', nIndex);
                if( -1 == nIndex )
                {
                    nIndex=nOldIndex;
                    sTemplate = sParams.getToken(0, ',', nIndex);
                }
            }
        }
    }
}

sal_uInt16 wwSectionManager::CurrentSectionColCount() const
{
    sal_uInt16 nIndexCols = 1;
    if (!maSegments.empty())
        nIndexCols = maSegments.back().maSep.ccolM1 + 1;
    return nIndexCols;
}

//Will there be a new pagebreak at this position (don't know what type
//until later)
bool wwSectionManager::WillHavePageDescHere(SwNodeIndex aIdx) const
{
    bool bRet = false;
    if (!maSegments.empty())
    {
        if (!maSegments.back().IsContinuous() &&
            maSegments.back().maStart == aIdx)
        {
            bRet = true;
        }
    }
    return bRet;
}

static sal_uInt16 lcl_GetMaxValidWordTOCLevel(const SwForm &rForm)
{
    // GetFormMax() returns level + 1, hence the -1
    sal_uInt16 nRet = rForm.GetFormMax()-1;

    // If the max of this type of TOC is greater than the max of a word
    // possible toc, then clip to the word max
    if (nRet > WW8ListManager::nMaxLevel)
        nRet = WW8ListManager::nMaxLevel;

    return nRet;
}

eF_ResT SwWW8ImplReader::Read_F_Tox( WW8FieldDesc* pF, OUString& rStr )
{
    if (!mbLoadingTOXCache)
    {
        mbLoadingTOXCache = true;
    }
    else
    {
        // Embedded TOX --> continue reading its content, but no further TOX
        // field
        ++mnEmbeddedTOXLevel;
        return FLD_TEXT;
    }

    if (pF->nLRes < 3)
        return FLD_TEXT;      // ignore (#i25440#)

    TOXTypes eTox;            // create a ToxBase
    switch( pF->nId )
    {
        case  8:
            eTox = TOX_INDEX;
            break;
        case 13:
            eTox = TOX_CONTENT;
            break;
        default:
            eTox = TOX_USER;
            break;
    }

    sal_uInt16 nCreateOf = (eTox == TOX_CONTENT) ? nsSwTOXElement::TOX_OUTLINELEVEL : nsSwTOXElement::TOX_MARK;

    sal_uInt16 nIndexCols = 1;

    const SwTOXType* pType = rDoc.GetTOXType( eTox, 0 );
    SwForm aOrigForm(eTox);
    SwTOXBase* pBase = new SwTOXBase( pType, aOrigForm, nCreateOf, OUString() );
    pBase->SetProtected(maSectionManager.CurrentSectionIsProtected());
    switch( eTox ){
    case TOX_INDEX:
        {
            sal_uInt16 eOptions = nsSwTOIOptions::TOI_SAME_ENTRY | nsSwTOIOptions::TOI_CASE_SENSITIVE;

            // TOX_OUTLINELEVEL setzen wir genau dann, wenn
            // die Parameter \o in 1 bis 9 liegen
            // oder der Parameter \f existiert
            // oder GARKEINE Switches Parameter angegeben sind.
            WW8ReadFieldParams aReadParam( rStr );
            for (;;)
            {
                const sal_Int32 nRet = aReadParam.SkipToNextToken();
                if ( nRet==-1 )
                    break;
                switch( nRet )
                {
                case 'c':
                    if ( aReadParam.GoToTokenParam() )
                    {
                        const OUString sParams( aReadParam.GetResult() );
                        // if NO OUString just ignore the \c
                        if( !sParams.isEmpty() )
                        {
                            nIndexCols = static_cast<sal_uInt16>(sParams.toInt32());
                        }
                    }
                    break;
                case 'e':
                    {
                        if ( aReadParam.GoToTokenParam() )  // if NO String just ignore the \e
                        {
                            OUString sDelimiter( aReadParam.GetResult() );
                            SwForm aForm( pBase->GetTOXForm() );

                            // Attention: if TOX_CONTENT brave
                            //            GetFormMax() returns MAXLEVEL + 1  !!
                            sal_uInt16 nEnd = aForm.GetFormMax()-1;

                            for(sal_uInt16 nLevel = 1;
                                   nLevel <= nEnd;
                                   ++nLevel)
                            {
                                // Levels count from 1
                                // Level 0 is reserved for CAPTION

                                // Delimiter statt Tabstop vor der Seitenzahl einsetzen,
                                // falls es eine Seitenzahl gibt:
                                FormTokenType ePrevType = TOKEN_END;
                                FormTokenType eType;
                                // -> #i21237#
                                SwFormTokens aPattern =
                                    aForm.GetPattern(nLevel);
                                SwFormTokens::iterator aIt = aPattern.begin();
                                do
                                {
                                    eType = ++aIt == aPattern.end() ? TOKEN_END : aIt->eTokenType;

                                    if (eType == TOKEN_PAGE_NUMS)
                                    {
                                        if (TOKEN_TAB_STOP == ePrevType)
                                        {
                                            --aIt;

                                            if(0x09 == sDelimiter[0])
                                                aIt->eTabAlign = SVX_TAB_ADJUST_END;
                                            else
                                            {
                                                SwFormToken aToken(TOKEN_TEXT);
                                                aToken.sText = sDelimiter;
                                                *aIt = aToken;
                                            }
                                            aForm.SetPattern(nLevel, aPattern);
                                        }

                                        eType = TOKEN_END;
                                    }

                                    ePrevType = eType;
                                }
                                while (TOKEN_END != eType);
                                // <- #i21237#
                            }
                            pBase->SetTOXForm( aForm );
                        }
                    }
                    break;
                case 'h':
                    {
                        eOptions |= nsSwTOIOptions::TOI_ALPHA_DELIMITTER;
                    }
                    break;
                }
            }
            pBase->SetOptions( eOptions );
        }
        break;

    case TOX_CONTENT:
        {
            bool bIsHyperlink = false;
            bool bShowPage = true;
            // TOX_OUTLINELEVEL setzen wir genau dann, wenn
            // die Parameter \o in 1 bis 9 liegen
            // oder der Parameter \f existiert
            // oder GARKEINE Switches Parameter angegeben sind.
            sal_uInt16 eCreateFrom = 0;
            sal_Int32 nMaxLevel = 0;
            WW8ReadFieldParams aReadParam( rStr );
            for (;;)
            {
                const sal_Int32 nRet = aReadParam.SkipToNextToken();
                if ( nRet==-1 )
                    break;
                switch( nRet )
                {
                case 'h':
                    bIsHyperlink = true;
                    break;
                case 'a':
                case 'c':
                    lcl_toxMatchACSwitch(*this, rDoc, *pBase, aReadParam,
                                           ('c' == nRet)
                                         ? CAPTION_COMPLETE
                                         : CAPTION_TEXT );
                    break;
                case 'o':
                    {
                        sal_Int32 nVal;
                        if( !aReadParam.GetTokenSttFromTo(0, &nVal, WW8ListManager::nMaxLevel) )
                            nVal = lcl_GetMaxValidWordTOCLevel(aOrigForm);
                        if( nMaxLevel < nVal )
                            nMaxLevel = nVal;
                        eCreateFrom |= nsSwTOXElement::TOX_OUTLINELEVEL;
                    }
                    break;
                case 'f':
                    eCreateFrom |= nsSwTOXElement::TOX_MARK;
                    break;
                case 'l':
                    {
                        sal_Int32 nVal;
                        if( aReadParam.GetTokenSttFromTo(0, &nVal, WW8ListManager::nMaxLevel) )
                        {
                            if( nMaxLevel < nVal )
                                nMaxLevel = nVal;
                            eCreateFrom |= nsSwTOXElement::TOX_MARK;
                        }
                    }
                    break;
                case 't': // paragraphs using special styles shall
                          // provide the TOX's content
                    lcl_toxMatchTSwitch(*this, *pBase, aReadParam);
                    eCreateFrom |= nsSwTOXElement::TOX_TEMPLATE;
                    break;
                case 'p':
                    {
                        if ( aReadParam.GoToTokenParam() )  // if NO String just ignore the \p
                        {
                            OUString sDelimiter( aReadParam.GetResult() );
                            SwForm aForm( pBase->GetTOXForm() );

                            // Attention: if TOX_CONTENT brave
                            //            GetFormMax() returns MAXLEVEL + 1  !!
                            sal_uInt16 nEnd = aForm.GetFormMax()-1;

                            for(sal_uInt16 nLevel = 1;
                                   nLevel <= nEnd;
                                   ++nLevel)
                            {
                                // Levels count from 1
                                // Level 0 is reserved for CAPTION

                                // Delimiter statt Tabstop vor der Seitenzahl einsetzen,
                                // falls es eine Seitenzahl gibt:
                                FormTokenType ePrevType = TOKEN_END;
                                FormTokenType eType;

                                // -> #i21237#
                                SwFormTokens aPattern = aForm.GetPattern(nLevel);
                                SwFormTokens::iterator aIt = aPattern.begin();
                                do
                                {
                                    eType = ++aIt == aPattern.end() ? TOKEN_END : aIt->eTokenType;

                                    if (eType == TOKEN_PAGE_NUMS)
                                    {
                                        if (TOKEN_TAB_STOP == ePrevType)
                                        {
                                            --aIt;

                                            SwFormToken aToken(TOKEN_TEXT);
                                            aToken.sText = sDelimiter;

                                            *aIt = aToken;
                                            aForm.SetPattern(nLevel,
                                                             aPattern);
                                        }
                                        eType = TOKEN_END;
                                    }
                                    ePrevType = eType;
                                }
                                while( TOKEN_END != eType );
                                // <- #i21237#
                            }
                            pBase->SetTOXForm( aForm );
                        }
                    }
                    break;
                case 'n': // don't print page numbers
                    {
                        // read START and END param
                        sal_Int32 nStart(0);
                        sal_Int32 nEnd(0);
                        if( !aReadParam.GetTokenSttFromTo(  &nStart, &nEnd,
                            WW8ListManager::nMaxLevel ) )
                        {
                            nStart = 1;
                            nEnd = aOrigForm.GetFormMax()-1;
                        }
                        // remove page numbers from this levels
                        SwForm aForm( pBase->GetTOXForm() );
                        if (aForm.GetFormMax() <= nEnd)
                            nEnd = aForm.GetFormMax()-1;
                        for ( sal_Int32 nLevel = nStart; nLevel<=nEnd; ++nLevel )
                        {
                            // Levels count from 1
                            // Level 0 is reserved for CAPTION

                            // Seitenzahl und ggfs. davorstehenden Tabstop
                            // entfernen:
                            FormTokenType eType;
                            // -> #i21237#
                            SwFormTokens aPattern = aForm.GetPattern(nLevel);
                            SwFormTokens::iterator aIt = aPattern.begin();
                            do
                            {
                                eType = ++aIt == aPattern.end() ? TOKEN_END : aIt->eTokenType;

                                if (eType == TOKEN_PAGE_NUMS)
                                {
                                    aIt = aPattern.erase(aIt);
                                    --aIt;
                                    if (
                                         TOKEN_TAB_STOP ==
                                         aIt->eTokenType
                                       )
                                    {
                                        aPattern.erase(aIt);
                                        aForm.SetPattern(nLevel, aPattern);
                                    }
                                    eType = TOKEN_END;
                                }
                            }
                            while (TOKEN_END != eType);
                            // <- #i21237#
                        }
                        pBase->SetTOXForm( aForm );
                    }
                    break;

                /*
                // the following switches are not (yet) supported
                // by good old StarWriter:
                case 'b':
                case 's':
                case 'd':
                    break;
                */
                }
            }

            // For loading the expression of TOC field, we need to mapping its parameters to TOX entries tokens
            // also include the hyperlinks and page references
            SwFormToken aLinkStart(TOKEN_LINK_START);
            SwFormToken aLinkEnd(TOKEN_LINK_END);
            aLinkStart.sCharStyleName = "Index Link";
            aLinkEnd.sCharStyleName = "Index Link";
            SwForm aForm(pBase->GetTOXForm());
            sal_uInt16 nEnd = aForm.GetFormMax()-1;

            for(sal_uInt16 nLevel = 1; nLevel <= nEnd; ++nLevel)
            {
                SwFormTokens aPattern = aForm.GetPattern(nLevel);
                if ( bIsHyperlink )
                {
                    aPattern.insert(aPattern.begin(), aLinkStart);
                }
                else if ( bShowPage )
                {
                    for (SwFormTokens::iterator aItr = aPattern.begin();aItr!= aPattern.end();++aItr)
                    {
                        if (aItr->eTokenType == TOKEN_PAGE_NUMS)
                        {
                            aPattern.insert(aItr,aLinkStart);
                            break;
                        }
                    }
                }
                aPattern.push_back(aLinkEnd);
                aForm.SetPattern(nLevel, aPattern);
            }
            pBase->SetTOXForm(aForm);

            if (!nMaxLevel)
                nMaxLevel = WW8ListManager::nMaxLevel;
            pBase->SetLevel(nMaxLevel);

            const TOXTypes eType = pBase->GetTOXType()->GetType();
            switch( eType )
            {
                case TOX_CONTENT:
                    {
                        //If we would be created from outlines, either explicitly or by default
                        //then see if we need extra styles added to the outlines
                        sal_uInt16 eEffectivelyFrom = eCreateFrom ? eCreateFrom : nsSwTOXElement::TOX_OUTLINELEVEL;
                        if (eEffectivelyFrom & nsSwTOXElement::TOX_OUTLINELEVEL)
                        {
                            // #i19683# Insert a text token " " between the number and entry token.
                            // In an ideal world we could handle the tab stop between the number and
                            // the entry correctly, but I currently have no clue how to obtain
                            // the tab stop position. It is _not_ set at the paragraph style.
                            SwForm* pForm = 0;
                            for (sal_uInt16 nI = 0; nI < vColl.size(); ++nI)
                            {
                                const SwWW8StyInf& rSI = vColl[nI];
                                if (rSI.IsOutlineNumbered())
                                {
                                    sal_uInt16 nStyleLevel = rSI.mnWW8OutlineLevel;
                                    const SwNumFmt& rFmt = rSI.GetOutlineNumrule()->Get( nStyleLevel );
                                    if ( SVX_NUM_NUMBER_NONE != rFmt.GetNumberingType() )
                                    {
                                        ++nStyleLevel;

                                        if ( !pForm )
                                            pForm = new SwForm( pBase->GetTOXForm() );

                                        SwFormTokens aPattern = pForm->GetPattern(nStyleLevel);
                                        SwFormTokens::iterator aIt =
                                                find_if(aPattern.begin(), aPattern.end(),
                                                SwFormTokenEqualToFormTokenType(TOKEN_ENTRY_NO));

                                        if ( aIt != aPattern.end() )
                                        {
                                            SwFormToken aNumberEntrySeparator( TOKEN_TEXT );
                                            aNumberEntrySeparator.sText = " ";
                                            aPattern.insert( ++aIt, aNumberEntrySeparator );
                                            pForm->SetPattern( nStyleLevel, aPattern );
                                        }
                                    }
                                }
                            }
                            if ( pForm )
                            {
                                pBase->SetTOXForm( *pForm );
                                delete pForm;
                            }
                        }

                        if (eCreateFrom)
                            pBase->SetCreate(eCreateFrom);
                        EnsureMaxLevelForTemplates(*pBase);
                    }
                    break;
                case TOX_ILLUSTRATIONS:
                    {
                        if( !eCreateFrom )
                            eCreateFrom = nsSwTOXElement::TOX_SEQUENCE;
                        pBase->SetCreate( eCreateFrom );

                        /*
                        We don't know until here if we are an illustration
                        or not, and so have being used a TOX_CONTENT so far
                        which has 10 levels, while TOX has only two, this
                        level is set only in the constructor of SwForm, so
                        create a new one and copy over anything that could
                        be set in the old one, and remove entries from the
                        pattern which do not apply to illustration indices
                        */
                        SwForm aOldForm( pBase->GetTOXForm() );
                        SwForm aNewForm( eType );
                        sal_uInt16 nNewEnd = aNewForm.GetFormMax()-1;

                        // #i21237#
                        for(sal_uInt16 nLevel = 1; nLevel <= nNewEnd; ++nLevel)
                        {
                            SwFormTokens aPattern = aOldForm.GetPattern(nLevel);
                            SwFormTokens::iterator new_end =
                                remove_if(aPattern.begin(), aPattern.end(), SwFormTokenEqualToFormTokenType(TOKEN_ENTRY_NO));
                            aPattern.erase(new_end, aPattern.end() ); // table index imported with wrong page number format
                            aForm.SetPattern( nLevel, aPattern );
                            aForm.SetTemplate( nLevel, aOldForm.GetTemplate(nLevel) );
                        }

                        pBase->SetTOXForm( aNewForm );
                    }
                    break;
                default:
                    OSL_ENSURE(!this, "Unhandled toc options!");
                    break;
            }
        }
        break;
    case TOX_USER:
        break;
    default:
        OSL_ENSURE(!this, "Unhandled toc options!");
        break;
    } // ToxBase fertig

    // #i21237# - propagate tab stops from paragraph styles used in TOX to patterns of the TOX
    pBase->AdjustTabStops( rDoc );

    //#i10028# inserting a toc implicltly acts like a parabreak in word and writer
    if ( pPaM->End() &&
         pPaM->End()->nNode.GetNode().GetTxtNode() &&
         pPaM->End()->nNode.GetNode().GetTxtNode()->Len() != 0 )
    {
        mbCareFirstParaEndInToc = true;
    }

    if (pPaM->GetPoint()->nContent.GetIndex())
        AppendTxtNode(*pPaM->GetPoint());

    const SwPosition* pPos = pPaM->GetPoint();

    SwFltTOX aFltTOX( pBase, nIndexCols );

    // test if there is already a break item on this node
    if(SwCntntNode* pNd = pPos->nNode.GetNode().GetCntntNode())
    {
        const SfxItemSet* pSet = pNd->GetpSwAttrSet();
        if( pSet )
        {
            if (SFX_ITEM_SET == pSet->GetItemState(RES_BREAK, false))
                aFltTOX.SetHadBreakItem(true);
            if (SFX_ITEM_SET == pSet->GetItemState(RES_PAGEDESC, false))
                aFltTOX.SetHadPageDescItem(true);
        }
    }

    //Will there be a new pagebreak at this position (don't know what type
    //until later)
    if (maSectionManager.WillHavePageDescHere(pPos->nNode))
        aFltTOX.SetHadPageDescItem(true);

    // Setze Anfang in Stack
    pReffedStck->NewAttr( *pPos, aFltTOX );

    rDoc.InsertTableOf(*pPaM->GetPoint(), *aFltTOX.GetBase());

    //The TOC field representation contents should be inserted into TOC section, but not after TOC section.
    //So we need update the document position when loading TOC representation and after loading TOC;
    if (mpPosAfterTOC)
    {
        delete mpPosAfterTOC;
    }
    mpPosAfterTOC = new SwPaM(*pPaM);
    (*pPaM).Move(fnMoveBackward);
    SwPaM aRegion(*pPaM);

    OSL_ENSURE(rDoc.GetCurTOX(*aRegion.GetPoint()), "Misunderstood how toc works");
    if (SwTOXBase* pBase2 = (SwTOXBase*)rDoc.GetCurTOX(*aRegion.GetPoint()))
    {
        pBase2->SetMSTOCExpression(rStr);

        if ( nIndexCols > 1 )
        {
            // Set the column number for index
            SfxItemSet aSet( rDoc.GetAttrPool(), RES_COL, RES_COL );
            SwFmtCol aCol;
            aCol.Init( nIndexCols, 708, USHRT_MAX );
            aSet.Put( aCol );
            pBase2->SetAttrSet( aSet );
        }

        // inserting a toc inserts a section before this point, so adjust pos
        // for future page/section segment insertion
        maSectionManager.PrependedInlineNode( *mpPosAfterTOC->GetPoint(), aRegion.GetNode() );
    }

    // Setze Ende in Stack
    pReffedStck->SetAttr( *pPos, RES_FLTR_TOX );

    if (!maApos.back()) //a para end in apo doesn't count
        bWasParaEnd = true;

    //Return FLD_TEXT, instead of FLD_OK
    //FLD_TEXT means the following content, commonly indicate the field representation content should be parsed
    //FLD_OK means the current field loading is finished. The rest part should be ignored.
    return FLD_TEXT;
}

eF_ResT SwWW8ImplReader::Read_F_Shape(WW8FieldDesc* /*pF*/, OUString& /*rStr*/)
{
    /*
    #i3958# 0x8 followed by 0x1 where the shape is the 0x8 and its anchoring
    to be ignored followed by a 0x1 with an empty drawing. Detect in inserting
    the drawing that we are in the Shape field and respond accordingly
    */
    return FLD_TEXT;
 }

eF_ResT SwWW8ImplReader::Read_F_Hyperlink( WW8FieldDesc* /*pF*/, OUString& rStr )
{
    OUString sURL, sTarget, sMark;
    bool bDataImport = false;
    //HYPERLINK "filename" [switches]

    rStr = comphelper::string::stripEnd(rStr, 1);

    if (!bDataImport)
    {
        bool bOptions = false;
        WW8ReadFieldParams aReadParam( rStr );
        for (;;)
        {
            const sal_Int32 nRet = aReadParam.SkipToNextToken();
            if ( nRet==-1 )
                break;
            switch( nRet )
            {
                case -2:
                    if (sURL.isEmpty() && !bOptions)
                        sURL = ConvertFFileName(aReadParam.GetResult());
                    break;

                case 'n':
                    sTarget = "_blank";
                    bOptions = true;
                    break;

                case 'l':
                    bOptions = true;
                    if ( aReadParam.SkipToNextToken()==-2 )
                    {
                        sMark = aReadParam.GetResult();
                        if( sMark.endsWith("\""))
                        {
                            sMark = sMark.copy( 0, sMark.getLength() - 1 );
                        }
                        // #120879# add cross reference bookmark name prefix, if it matches internal TOC bookmark naming convention
                        if ( IsTOCBookmarkName( sMark ) )
                        {
                            OUString sTmp = IDocumentMarkAccess::GetCrossRefHeadingBookmarkNamePrefix();
                            sTmp += sMark;
                            sMark = sTmp;
                            // track <sMark> as referenced TOC bookmark.
                            pReffedStck->aReferencedTOCBookmarks.insert( sMark );
                        }

                        if (mbLoadingTOXCache)
                        {
                            mbLoadingTOXHyperlink = true; //on loading a TOC field nested hyperlink field
                        }
                    }
                    break;
                case 't':
                    bOptions = true;
                    if ( aReadParam.SkipToNextToken()==-2 )
                        sTarget = aReadParam.GetResult();
                    break;
                case 'h':
                case 'm':
                    OSL_ENSURE( !this, "Auswertung fehlt noch - Daten unbekannt" );
                    //fall-through
                case 's':   //worthless fake anchor option
                    bOptions = true;
                    break;
            }
        }
    }

    // das Resultat uebernehmen
   OSL_ENSURE(!sURL.isEmpty() || !sMark.isEmpty(), "WW8: Empty URL");

    if( !sMark.isEmpty() )
        ( sURL += "#" ) += sMark;

    SwFmtINetFmt aURL(sURL, sTarget);
    // If on loading TOC field, change the default style into the "index link"
    if (mbLoadingTOXCache)
    {
        OUString sLinkStyle("Index Link");
        sal_uInt16 nPoolId =
            SwStyleNameMapper::GetPoolIdFromUIName( sLinkStyle, nsSwGetPoolIdFromName::GET_POOLID_CHRFMT );
        aURL.SetVisitedFmtAndId( sLinkStyle, nPoolId );
        aURL.SetINetFmtAndId( sLinkStyle, nPoolId );
    }

    //As an attribute this needs to be closed, and that'll happen from
    //EndExtSprm in conjunction with the maFieldStack If there are are flyfrms
    //between the start and begin, their hyperlinks will be set at that time
    //as well.
    pCtrlStck->NewAttr( *pPaM->GetPoint(), aURL );
    return FLD_TEXT;
}

static void lcl_ImportTox(SwDoc &rDoc, SwPaM &rPaM, const OUString &rStr, bool bIdx)
{
    TOXTypes eTox = ( !bIdx ) ? TOX_CONTENT : TOX_INDEX;    // Default

    sal_uInt16 nLevel = 1;

    OUString sFldTxt;
    WW8ReadFieldParams aReadParam(rStr);
    for (;;)
    {
        const sal_Int32 nRet = aReadParam.SkipToNextToken();
        if ( nRet==-1 )
            break;
        switch( nRet )
        {
        case -2:
            if( sFldTxt.isEmpty() )
            {
                // PrimaryKey ohne ":", 2nd dahinter
                sFldTxt = aReadParam.GetResult();
            }
            break;

        case 'f':
            if ( aReadParam.GoToTokenParam() )
            {
                const OUString sParams( aReadParam.GetResult() );
                if( sParams[0]!='C' && sParams[0]!='c' )
                    eTox = TOX_USER;
            }
            break;

        case 'l':
            if ( aReadParam.GoToTokenParam() )
            {
                const OUString sParams( aReadParam.GetResult() );
                // if NO String just ignore the \l
                if( !sParams.isEmpty() && sParams[0]>'0' && sParams[0]<='9' )
                {
                    nLevel = (sal_uInt16)sParams.toInt32();
                }
            }
            break;
        }
    }

    OSL_ENSURE( rDoc.GetTOXTypeCount( eTox ), "Doc.GetTOXTypeCount() == 0  :-(" );

    const SwTOXType* pT = rDoc.GetTOXType( eTox, 0 );
    SwTOXMark aM( pT );

    if( eTox != TOX_INDEX )
        aM.SetLevel( nLevel );
    else
    {
        sal_Int32 nFnd = sFldTxt.indexOf( WW8_TOX_LEVEL_DELIM );
        if( -1 != nFnd )  // it exist levels
        {
            aM.SetPrimaryKey( sFldTxt.copy( 0, nFnd ) );
            sal_Int32 nScndFnd = sFldTxt.indexOf( WW8_TOX_LEVEL_DELIM, nFnd+1 );
            if( -1 != nScndFnd )
            {
                aM.SetSecondaryKey(  sFldTxt.copy( nFnd+1, nScndFnd - nFnd - 1 ));
                nFnd = nScndFnd;
            }
            sFldTxt = sFldTxt.copy( nFnd+1 );
        }
    }

    if (!sFldTxt.isEmpty())
    {
        aM.SetAlternativeText( sFldTxt );
        rDoc.getIDocumentContentOperations().InsertPoolItem( rPaM, aM, 0 );
    }
}

void sw::ms::ImportXE(SwDoc &rDoc, SwPaM &rPaM, const OUString &rStr)
{
    lcl_ImportTox(rDoc, rPaM, rStr, true);
}

void SwWW8ImplReader::ImportTox( int nFldId, const OUString& aStr )
{
    bool bIdx = (nFldId != 9);
    lcl_ImportTox(rDoc, *pPaM, aStr, bIdx);
}

void SwWW8ImplReader::Read_FldVanish( sal_uInt16, const sal_uInt8*, short nLen )
{
    //Meaningless in a style
    if (pAktColl || !pPlcxMan)
        return;

    const int nChunk = 64;  //number of characters to read at one time

    // Vorsicht: Bei Feldnamen mit Umlauten geht das MEMICMP nicht!
    const static sal_Char *aFldNames[] = {  "\x06""INHALT", "\x02""XE", // dt.
                                            "\x02""TC"  };              // us
    const static sal_uInt8  aFldId[] = { 9, 4, 9 };

    if( nLen < 0 )
    {
        bIgnoreText = false;
        return;
    }

    // our methode was called from
    // ''Skip attributes of field contents'' loop within ReadTextAttr()
    if( bIgnoreText )
        return;

    bIgnoreText = true;
    long nOldPos = pStrm->Tell();

    WW8_CP nStartCp = pPlcxMan->Where() + pPlcxMan->GetCpOfs();

    OUString sFieldName;
    sal_Int32 nFieldLen = pSBase->WW8ReadString( *pStrm, sFieldName, nStartCp,
        nChunk, eStructCharSet );
    nStartCp+=nFieldLen;

    sal_Int32 nC = 0;
    //If the first chunk did not start with a field start then
    //reset the stream position and give up
    if( !nFieldLen || sFieldName[nC]!=0x13 ) // Field Start Mark
    {
        // If Field End Mark found
        if( nFieldLen && sFieldName[nC]==0x15 )
            bIgnoreText = false;
        pStrm->Seek( nOldPos );
        return;                 // kein Feld zu finden
    }

    sal_Int32 nFnd;
    //If this chunk does not contain a field end, keep reading chunks
    //until we find one, or we run out of text,
    for (;;)
    {
        nFnd = sFieldName.indexOf(0x15);
        //found field end, we can stop now
        if (nFnd != -1)
            break;
        OUString sTemp;
        nFieldLen = pSBase->WW8ReadString( *pStrm, sTemp,
                                           nStartCp, nChunk, eStructCharSet );
        sFieldName+=sTemp;
        nStartCp+=nFieldLen;
        if (!nFieldLen)
            break;
    }

    pStrm->Seek( nOldPos );

    //if we have no 0x15 give up, otherwise erase everything from the 0x15
    //onwards
    if (nFnd<0)
        return;

    sFieldName = sFieldName.copy(0, nFnd);

    nC++;
    while ( sFieldName[nC]==' ' )
        nC++;

    for( int i = 0; i < 3; i++ )
    {
        const sal_Char* pName = aFldNames[i];
        const sal_Int32 nNameLen = static_cast<sal_Int32>(*pName++);
        if( sFieldName.matchIgnoreAsciiCaseAsciiL( pName, nNameLen, nC ) )
        {
            ImportTox( aFldId[i], sFieldName.copy( nC + nNameLen ) );
            break;                  // keine Mehrfachnennungen moeglich
        }
    }
    bIgnoreText = true;
    pStrm->Seek( nOldPos );
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
