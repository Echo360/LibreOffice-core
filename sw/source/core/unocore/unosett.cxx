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

#include <svx/svxids.hrc>
#include <editeng/memberids.hrc>
#include <swtypes.hxx>
#include <cmdid.h>
#include <hintids.hxx>
#include "poolfmt.hrc"
#include "poolfmt.hxx"
#include <fmtcol.hxx>
#include <unomap.hxx>
#include <unostyle.hxx>
#include <unosett.hxx>
#include <unoprnms.hxx>
#include <ftninfo.hxx>
#include <doc.hxx>
#include <pagedesc.hxx>
#include <charfmt.hxx>
#include <lineinfo.hxx>
#include <docsh.hxx>
#include <docary.hxx>
#include <docstyle.hxx>
#include <fmtclds.hxx>
#include <editeng/brushitem.hxx>
#include <com/sun/star/text/XFootnotesSettingsSupplier.hpp>
#include <com/sun/star/text/XFootnote.hpp>
#include <com/sun/star/text/XFootnotesSupplier.hpp>
#include <com/sun/star/text/XEndnotesSupplier.hpp>
#include <com/sun/star/text/XEndnotesSettingsSupplier.hpp>
#include <com/sun/star/text/FootnoteNumbering.hpp>
#include <com/sun/star/text/HoriOrientation.hpp>
#include <com/sun/star/style/LineNumberPosition.hpp>
#include <com/sun/star/awt/XBitmap.hpp>
#include <com/sun/star/beans/PropertyAttribute.hpp>
#include <com/sun/star/style/VerticalAlignment.hpp>
#include <vcl/font.hxx>
#include <editeng/flstitem.hxx>
#include <vcl/metric.hxx>
#include <svtools/ctrltool.hxx>
#include <osl/mutex.hxx>
#include <vcl/svapp.hxx>
#include <toolkit/helper/vclunohelper.hxx>
#include <editeng/unofdesc.hxx>
#include <fmtornt.hxx>
#include <SwStyleNameMapper.hxx>
#include <com/sun/star/text/PositionAndSpaceMode.hpp>
#include <com/sun/star/text/LabelFollow.hpp>
#include <numrule.hxx>
#include <comphelper/servicehelper.hxx>
#include <cppuhelper/supportsservice.hxx>
#include <paratr.hxx>

using namespace ::com::sun::star;
using namespace ::com::sun::star::uno;
using namespace ::com::sun::star::lang;
using namespace ::com::sun::star::beans;
using namespace ::com::sun::star::text;
using namespace ::com::sun::star::style;

struct PropValData
{
    uno::Any        aVal;
    OUString            sPropName;
    PropValData(void* pVal, const char* cPropName, uno::Type aType ) :
        aVal(pVal, aType),
        sPropName(OUString::createFromAscii(cPropName))
        {}
    PropValData(const uno::Any& rVal, const OUString& rPropName) :
        aVal(rVal),
        sPropName(rPropName)
        {}
};

// Constants for the css::text::ColumnSeparatorStyle
#define API_COL_LINE_NONE               0
#define API_COL_LINE_SOLID              1
#define API_COL_LINE_DOTTED             2
#define API_COL_LINE_DASHED             3

typedef std::vector<PropValData*> PropValDataArr;

#define WID_PREFIX                      0
#define WID_SUFFIX                      1
#define WID_NUMBERING_TYPE              2
#define WID_START_AT                    3
#define WID_FOOTNOTE_COUNTING           4
#define WID_PARAGRAPH_STYLE             5
#define WID_PAGE_STYLE                  6
#define WID_CHARACTER_STYLE             7
#define WID_POSITION_END_OF_DOC         8
#define WID_END_NOTICE                  9
#define WID_BEGIN_NOTICE                10
#define WID_ANCHOR_CHARACTER_STYLE      11

const SfxItemPropertySet* GetFootnoteSet()
{
    static const SfxItemPropertyMapEntry aFootnoteMap_Impl[] =
    {
        { OUString(UNO_NAME_ANCHOR_CHAR_STYLE_NAME),WID_ANCHOR_CHARACTER_STYLE, ::cppu::UnoType<OUString>::get(), PROPERTY_NONE,     0},
        { OUString(UNO_NAME_BEGIN_NOTICE),          WID_BEGIN_NOTICE,       ::cppu::UnoType<OUString>::get(), PROPERTY_NONE,     0},
        { OUString(UNO_NAME_CHAR_STYLE_NAME),       WID_CHARACTER_STYLE,    ::cppu::UnoType<OUString>::get(), PROPERTY_NONE,     0},
        { OUString(UNO_NAME_END_NOTICE),            WID_END_NOTICE ,        ::cppu::UnoType<OUString>::get(), PROPERTY_NONE,     0},
        { OUString(UNO_NAME_FOOTNOTE_COUNTING),     WID_FOOTNOTE_COUNTING,  ::cppu::UnoType<sal_Int16>::get(), PROPERTY_NONE,     0},
        { OUString(UNO_NAME_NUMBERING_TYPE),        WID_NUMBERING_TYPE,     ::cppu::UnoType<sal_Int16>::get(), PROPERTY_NONE,         0},
        { OUString(UNO_NAME_PAGE_STYLE_NAME),       WID_PAGE_STYLE,         ::cppu::UnoType<OUString>::get(), PROPERTY_NONE,     0},
        { OUString(UNO_NAME_PARA_STYLE_NAME),       WID_PARAGRAPH_STYLE,    ::cppu::UnoType<OUString>::get(), PROPERTY_NONE,     0},
        { OUString(UNO_NAME_POSITION_END_OF_DOC),   WID_POSITION_END_OF_DOC,::getBooleanCppuType(), PROPERTY_NONE,         0},
        { OUString(UNO_NAME_PREFIX),                WID_PREFIX,             ::cppu::UnoType<OUString>::get(), PROPERTY_NONE,     0},
        { OUString(UNO_NAME_START_AT),              WID_START_AT ,          ::cppu::UnoType<sal_Int16>::get(), PROPERTY_NONE,         0},
        { OUString(UNO_NAME_SUFFIX),                WID_SUFFIX,             ::cppu::UnoType<OUString>::get(), PROPERTY_NONE,     0},
        { OUString(), 0, css::uno::Type(), 0, 0 }
    };
    static const SfxItemPropertySet aFootnoteSet_Impl(aFootnoteMap_Impl);
    return &aFootnoteSet_Impl;
}

const SfxItemPropertySet* GetEndnoteSet()
{
    static const SfxItemPropertyMapEntry aEndnoteMap_Impl[] =
    {
        { OUString(UNO_NAME_ANCHOR_CHAR_STYLE_NAME),WID_ANCHOR_CHARACTER_STYLE, ::cppu::UnoType<OUString>::get(), PROPERTY_NONE,     0},
        { OUString(UNO_NAME_CHAR_STYLE_NAME),       WID_CHARACTER_STYLE,    ::cppu::UnoType<OUString>::get(), PROPERTY_NONE,     0},
        { OUString(UNO_NAME_NUMBERING_TYPE),        WID_NUMBERING_TYPE,     ::cppu::UnoType<sal_Int16>::get(), PROPERTY_NONE,         0},
        { OUString(UNO_NAME_PAGE_STYLE_NAME),       WID_PAGE_STYLE,         ::cppu::UnoType<OUString>::get(), PROPERTY_NONE,     0},
        { OUString(UNO_NAME_PARA_STYLE_NAME),       WID_PARAGRAPH_STYLE,    ::cppu::UnoType<OUString>::get(), PROPERTY_NONE,     0},
        { OUString(UNO_NAME_PREFIX),                WID_PREFIX,     ::cppu::UnoType<OUString>::get(), PROPERTY_NONE,     0},
        { OUString(UNO_NAME_START_AT),              WID_START_AT ,          ::cppu::UnoType<sal_Int16>::get(), PROPERTY_NONE,         0},
        { OUString(UNO_NAME_SUFFIX),                WID_SUFFIX,     ::cppu::UnoType<OUString>::get(), PROPERTY_NONE,     0},
        { OUString(), 0, css::uno::Type(), 0, 0 }
    };
    static const SfxItemPropertySet aEndnoteSet_Impl(aEndnoteMap_Impl);
    return &aEndnoteSet_Impl;
}

const SfxItemPropertySet* GetNumberingRulesSet()
{
    static const SfxItemPropertyMapEntry aNumberingRulesMap_Impl[] =
    {
        { OUString(UNO_NAME_IS_ABSOLUTE_MARGINS),       WID_IS_ABS_MARGINS, ::getBooleanCppuType(),            PROPERTY_NONE,     0},
        { OUString(UNO_NAME_IS_AUTOMATIC),              WID_IS_AUTOMATIC,   ::getBooleanCppuType(),            PROPERTY_NONE,     0},
        { OUString(UNO_NAME_IS_CONTINUOUS_NUMBERING),   WID_CONTINUOUS,     ::getBooleanCppuType(),            PROPERTY_NONE,     0},
        { OUString(UNO_NAME_NAME),                      WID_RULE_NAME   ,   ::cppu::UnoType<OUString>::get(), PropertyAttribute::READONLY,     0},
        { OUString(UNO_NAME_NUMBERING_IS_OUTLINE),      WID_IS_OUTLINE, ::getBooleanCppuType(),            PROPERTY_NONE,     0},
        { OUString(UNO_NAME_DEFAULT_LIST_ID),           WID_DEFAULT_LIST_ID, ::cppu::UnoType<OUString>::get(), PropertyAttribute::READONLY, 0},
        { OUString(), 0, css::uno::Type(), 0, 0 }
    };
    static const SfxItemPropertySet  aNumberingRulesSet_Impl( aNumberingRulesMap_Impl );
    return &aNumberingRulesSet_Impl;
}

#define WID_NUM_ON                      0
#define WID_SEPARATOR_INTERVAL          1
#define WID_NUMBERING_TYPE              2
#define WID_NUMBER_POSITION             3
#define WID_DISTANCE                    4
#define WID_INTERVAL                    5
#define WID_SEPARATOR_TEXT              6
#define WID_COUNT_EMPTY_LINES           8
#define WID_COUNT_LINES_IN_FRAMES       9
#define WID_RESTART_AT_EACH_PAGE        10

const SfxItemPropertySet* GetLineNumberingSet()
{
    static const SfxItemPropertyMapEntry aLineNumberingMap_Impl[] =
    {
        { OUString(UNO_NAME_CHAR_STYLE_NAME),         WID_CHARACTER_STYLE,    ::cppu::UnoType<OUString>::get(), PROPERTY_NONE,     0},
        { OUString(UNO_NAME_COUNT_EMPTY_LINES),       WID_COUNT_EMPTY_LINES , ::getBooleanCppuType(),PROPERTY_NONE,     0},
        { OUString(UNO_NAME_COUNT_LINES_IN_FRAMES),   WID_COUNT_LINES_IN_FRAMES, ::getBooleanCppuType(),PROPERTY_NONE,     0},
        { OUString(UNO_NAME_DISTANCE),                WID_DISTANCE       ,    ::cppu::UnoType<sal_Int32>::get(),PROPERTY_NONE,     0},
        { OUString(UNO_NAME_IS_ON),                   WID_NUM_ON,             ::getBooleanCppuType()  ,          PROPERTY_NONE,     0},
        { OUString(UNO_NAME_INTERVAL),                WID_INTERVAL  ,       ::cppu::UnoType<sal_Int16>::get(),PROPERTY_NONE,     0},
        { OUString(UNO_NAME_SEPARATOR_TEXT),          WID_SEPARATOR_TEXT,   ::cppu::UnoType<OUString>::get(), PROPERTY_NONE,     0},
        { OUString(UNO_NAME_NUMBER_POSITION),         WID_NUMBER_POSITION,    ::cppu::UnoType<sal_Int16>::get(),PROPERTY_NONE,     0},
        { OUString(UNO_NAME_NUMBERING_TYPE),          WID_NUMBERING_TYPE ,    ::cppu::UnoType<sal_Int16>::get(),PROPERTY_NONE,     0},
        { OUString(UNO_NAME_RESTART_AT_EACH_PAGE),    WID_RESTART_AT_EACH_PAGE, ::getBooleanCppuType()  ,          PROPERTY_NONE,     0},
        { OUString(UNO_NAME_SEPARATOR_INTERVAL),      WID_SEPARATOR_INTERVAL, ::cppu::UnoType<sal_Int16>::get(),PROPERTY_NONE,     0},
        { OUString(), 0, css::uno::Type(), 0, 0 }
    };
    static const SfxItemPropertySet aLineNumberingSet_Impl(aLineNumberingMap_Impl);
    return &aLineNumberingSet_Impl;
}

static SwCharFmt* lcl_getCharFmt(SwDoc* pDoc, const uno::Any& aValue)
{
    SwCharFmt* pRet = 0;
    OUString uTmp;
    aValue >>= uTmp;
    OUString sCharFmt;
    SwStyleNameMapper::FillUIName(uTmp, sCharFmt, nsSwGetPoolIdFromName::GET_POOLID_CHRFMT, true);
    if (sCharFmt != SW_RESSTR(STR_POOLCOLL_STANDARD))
    {
        pRet = pDoc->FindCharFmtByName( sCharFmt );
    }
    if(!pRet)
    {
        const sal_uInt16 nId = SwStyleNameMapper::GetPoolIdFromUIName(sCharFmt, nsSwGetPoolIdFromName::GET_POOLID_CHRFMT);
        if(USHRT_MAX != nId)
            pRet = pDoc->GetCharFmtFromPool( nId );
    }
    return pRet;
}

static SwTxtFmtColl* lcl_GetParaStyle(SwDoc* pDoc, const uno::Any& aValue)
{
    OUString uTmp;
    aValue >>= uTmp;
    OUString sParaStyle;
    SwStyleNameMapper::FillUIName(uTmp, sParaStyle, nsSwGetPoolIdFromName::GET_POOLID_TXTCOLL, true );
    SwTxtFmtColl* pRet = pDoc->FindTxtFmtCollByName( sParaStyle );
    if( !pRet  )
    {
        const sal_uInt16 nId = SwStyleNameMapper::GetPoolIdFromUIName( sParaStyle, nsSwGetPoolIdFromName::GET_POOLID_TXTCOLL );
        if( USHRT_MAX != nId  )
            pRet = pDoc->GetTxtCollFromPool( nId );
    }
    return pRet;
}

static SwPageDesc* lcl_GetPageDesc(SwDoc* pDoc, const uno::Any& aValue)
{
    SwPageDesc* pRet = 0;
    const sal_uInt16 nCount = pDoc->GetPageDescCnt();
    OUString uTmp;
    aValue >>= uTmp;
    OUString sPageDesc;
    SwStyleNameMapper::FillUIName(uTmp, sPageDesc, nsSwGetPoolIdFromName::GET_POOLID_PAGEDESC, true );
    for( sal_uInt16 i = 0; i < nCount; i++)
    {
        const SwPageDesc& rDesc = pDoc->GetPageDesc( i );
        if(rDesc.GetName() == sPageDesc)
        {
            pRet = (SwPageDesc*)&rDesc;
            break;
        }
    }
    if(!pRet)
    {
        const sal_uInt16 nId = SwStyleNameMapper::GetPoolIdFromUIName(sPageDesc, nsSwGetPoolIdFromName::GET_POOLID_PAGEDESC);
        if(USHRT_MAX != nId)
            pRet = pDoc->GetPageDescFromPool( nId );
    }
    return pRet;
}

// Numbering
const unsigned short aSvxToUnoAdjust[] =
{
    text::HoriOrientation::LEFT,    //3
    text::HoriOrientation::RIGHT,  //1
    USHRT_MAX,
    text::HoriOrientation::CENTER, //2
    USHRT_MAX,
    USHRT_MAX
};

const unsigned short aUnoToSvxAdjust[] =
{
    USHRT_MAX,
    SVX_ADJUST_RIGHT,       // 1
    SVX_ADJUST_CENTER,      // 3
    SVX_ADJUST_LEFT,        // 0
    USHRT_MAX,
    USHRT_MAX
};

OUString SwXFootnoteProperties::getImplementationName(void) throw( RuntimeException, std::exception )
{
    return OUString("SwXFootnoteProperties");
}

sal_Bool SwXFootnoteProperties::supportsService(const OUString& rServiceName) throw( RuntimeException, std::exception )
{
    return cppu::supportsService(this, rServiceName);
}

Sequence< OUString > SwXFootnoteProperties::getSupportedServiceNames(void) throw( RuntimeException, std::exception )
{
    Sequence< OUString > aRet(1);
    OUString* pArray = aRet.getArray();
    pArray[0] = "com.sun.star.text.FootnoteSettings";
    return aRet;
}

SwXFootnoteProperties::SwXFootnoteProperties(SwDoc* pDc) :
    pDoc(pDc),
    m_pPropertySet(GetFootnoteSet())
{
}

SwXFootnoteProperties::~SwXFootnoteProperties()
{

}

uno::Reference< beans::XPropertySetInfo >  SwXFootnoteProperties::getPropertySetInfo(void)
                                                                throw( uno::RuntimeException, std::exception )
{
    static uno::Reference< beans::XPropertySetInfo >  aRef = m_pPropertySet->getPropertySetInfo();
    return aRef;
}

void SwXFootnoteProperties::setPropertyValue(const OUString& rPropertyName, const uno::Any& aValue)
    throw( beans::UnknownPropertyException, beans::PropertyVetoException, lang::IllegalArgumentException, lang::WrappedTargetException, uno::RuntimeException, std::exception )
{
    SolarMutexGuard aGuard;
    if(pDoc)
    {
        const SfxItemPropertySimpleEntry*  pEntry = m_pPropertySet->getPropertyMap().getByName( rPropertyName );
        if(pEntry)
        {
            if ( pEntry->nFlags & PropertyAttribute::READONLY)
                throw PropertyVetoException("Property is read-only: " + rPropertyName, static_cast < cppu::OWeakObject * > ( this ) );
            SwFtnInfo aFtnInfo(pDoc->GetFtnInfo());
            switch(pEntry->nWID)
            {
                case WID_PREFIX:
                {
                    OUString uTmp;
                    aValue >>= uTmp;
                    aFtnInfo.SetPrefix(uTmp);
                }
                break;
                case WID_SUFFIX:
                {
                    OUString uTmp;
                    aValue >>= uTmp;
                    aFtnInfo.SetSuffix(uTmp);
                }
                break;
                case WID_NUMBERING_TYPE:
                {
                    sal_Int16 nTmp = 0;
                    aValue >>= nTmp;
                    if(nTmp >= 0 &&
                        (nTmp <= SVX_NUM_ARABIC ||
                            nTmp > SVX_NUM_BITMAP))
                        aFtnInfo.aFmt.SetNumberingType(nTmp);
                    else
                        throw lang::IllegalArgumentException();
                }
                break;
                case WID_START_AT:
                {
                    sal_Int16 nTmp = 0;
                    aValue >>= nTmp;
                    aFtnInfo.nFtnOffset = nTmp;
                }
                break;
                case WID_FOOTNOTE_COUNTING:
                {
                    sal_Int16 nTmp = 0;
                    aValue >>= nTmp;
                    switch(nTmp)
                    {
                        case  FootnoteNumbering::PER_PAGE:
                            aFtnInfo.eNum = FTNNUM_PAGE;
                        break;
                        case  FootnoteNumbering::PER_CHAPTER:
                            aFtnInfo.eNum = FTNNUM_CHAPTER;
                        break;
                        case  FootnoteNumbering::PER_DOCUMENT:
                            aFtnInfo.eNum = FTNNUM_DOC;
                        break;
                    }
                }
                break;
                case WID_PARAGRAPH_STYLE:
                {
                    SwTxtFmtColl* pColl = lcl_GetParaStyle(pDoc, aValue);
                    if(pColl)
                        aFtnInfo.SetFtnTxtColl(*pColl);
                }
                break;
                case WID_PAGE_STYLE:
                {
                    SwPageDesc* pDesc = lcl_GetPageDesc(pDoc, aValue);
                    if(pDesc)
                        aFtnInfo.ChgPageDesc( pDesc );
                }
                break;
                case WID_ANCHOR_CHARACTER_STYLE:
                case WID_CHARACTER_STYLE:
                {
                    SwCharFmt* pFmt = lcl_getCharFmt(pDoc, aValue);
                    if(pFmt)
                    {
                        if(pEntry->nWID == WID_ANCHOR_CHARACTER_STYLE)
                            aFtnInfo.SetAnchorCharFmt(pFmt);
                        else
                            aFtnInfo.SetCharFmt(pFmt);
                    }
                }
                break;
                case WID_POSITION_END_OF_DOC:
                {
                    bool bVal = *(sal_Bool*)aValue.getValue();
                    aFtnInfo.ePos = bVal ? FTNPOS_CHAPTER : FTNPOS_PAGE;
                }
                break;
                case WID_END_NOTICE:
                {
                    OUString uTmp;
                    aValue >>= uTmp;
                    aFtnInfo.aQuoVadis = uTmp;
                }
                break;
                case WID_BEGIN_NOTICE:
                {
                    OUString uTmp;
                    aValue >>= uTmp;
                    aFtnInfo.aErgoSum = uTmp;
                }
                break;
            }
            pDoc->SetFtnInfo(aFtnInfo);
        }
        else
            throw beans::UnknownPropertyException("Unknown property: " + rPropertyName, static_cast < cppu::OWeakObject * > ( this ) );
    }
    else
        throw uno::RuntimeException();
}

uno::Any SwXFootnoteProperties::getPropertyValue(const OUString& rPropertyName)
    throw( beans::UnknownPropertyException, lang::WrappedTargetException, uno::RuntimeException, std::exception )
{
    SolarMutexGuard aGuard;
    uno::Any aRet;
    if(pDoc)
    {
        const SfxItemPropertySimpleEntry*  pEntry = m_pPropertySet->getPropertyMap().getByName( rPropertyName );
        if(pEntry)
        {
            const SwFtnInfo& rFtnInfo = pDoc->GetFtnInfo();
            switch(pEntry->nWID)
            {
                case WID_PREFIX:
                {
                    aRet <<= rFtnInfo.GetPrefix();
                }
                break;
                case WID_SUFFIX:
                {
                    aRet <<= rFtnInfo.GetSuffix();
                }
                break;
                case  WID_NUMBERING_TYPE :
                {
                    aRet <<= rFtnInfo.aFmt.GetNumberingType();
                }
                break;
                case  WID_START_AT:
                    aRet <<= (sal_Int16)rFtnInfo.nFtnOffset;
                break;
                case  WID_FOOTNOTE_COUNTING  :
                {
                    sal_Int16 nRet = 0;
                    switch(rFtnInfo.eNum)
                    {
                        case  FTNNUM_PAGE:
                            nRet = FootnoteNumbering::PER_PAGE;
                        break;
                        case  FTNNUM_CHAPTER:
                            nRet = FootnoteNumbering::PER_CHAPTER;
                        break;
                        case  FTNNUM_DOC:
                            nRet = FootnoteNumbering::PER_DOCUMENT;
                        break;
                    }
                    aRet <<= nRet;
                }
                break;
                case  WID_PARAGRAPH_STYLE    :
                {
                    SwTxtFmtColl* pColl = rFtnInfo.GetFtnTxtColl();
                    OUString aString;
                    if(pColl)
                        aString = pColl->GetName();
                    SwStyleNameMapper::FillProgName(aString, aString, nsSwGetPoolIdFromName::GET_POOLID_TXTCOLL, true);
                    aRet <<= aString;
                }
                break;
                case  WID_PAGE_STYLE :
                {
                    OUString aString;
                    if( rFtnInfo.KnowsPageDesc() )
                    {
                        SwStyleNameMapper::FillProgName(
                                rFtnInfo.GetPageDesc( *pDoc )->GetName(),
                                aString,
                                nsSwGetPoolIdFromName::GET_POOLID_PAGEDESC,
                                true);
                    }
                    aRet <<= aString;
                }
                break;
                case WID_ANCHOR_CHARACTER_STYLE:
                case WID_CHARACTER_STYLE:
                {
                    OUString aString;
                    const SwCharFmt* pCharFmt = 0;
                    if( pEntry->nWID == WID_ANCHOR_CHARACTER_STYLE )
                    {
                        if( rFtnInfo.GetAnchorCharFmtDep()->GetRegisteredIn() )
                            pCharFmt = rFtnInfo.GetAnchorCharFmt(*pDoc);
                    }
                    else
                    {
                        if( rFtnInfo.GetCharFmtDep()->GetRegisteredIn() )
                            pCharFmt = rFtnInfo.GetCharFmt(*pDoc);
                    }
                    if( pCharFmt )
                    {
                        SwStyleNameMapper::FillProgName(
                                pCharFmt->GetName(),
                                aString,
                                nsSwGetPoolIdFromName::GET_POOLID_CHRFMT,
                                true);
                    }
                    aRet <<= aString;
                }
                break;
                case  WID_POSITION_END_OF_DOC:
                {
                    sal_Bool bTemp = FTNPOS_CHAPTER == rFtnInfo.ePos;
                    aRet.setValue(&bTemp, ::getCppuBooleanType());
                }
                break;
                case  WID_END_NOTICE         :
                    aRet <<= rFtnInfo.aQuoVadis;
                break;
                case  WID_BEGIN_NOTICE       :
                    aRet <<= rFtnInfo.aErgoSum;
                break;
            }
        }
        else
            throw UnknownPropertyException("Unknown property: " + rPropertyName, static_cast < cppu::OWeakObject * > ( this ) );
    }
    else
        throw uno::RuntimeException();
    return aRet;
}

void SwXFootnoteProperties::addPropertyChangeListener(
    const OUString& /*rPropertyName*/, const uno::Reference< beans::XPropertyChangeListener > & /*xListener*/)
        throw( beans::UnknownPropertyException, lang::WrappedTargetException, uno::RuntimeException, std::exception )
{
    OSL_FAIL("not implemented");
}

void SwXFootnoteProperties::removePropertyChangeListener(
    const OUString& /*rPropertyName*/, const uno::Reference< beans::XPropertyChangeListener > & /*xListener*/)
        throw( beans::UnknownPropertyException, lang::WrappedTargetException, uno::RuntimeException, std::exception )
{
    OSL_FAIL("not implemented");
}

void SwXFootnoteProperties::addVetoableChangeListener(
    const OUString& /*rPropertyName*/, const uno::Reference< beans::XVetoableChangeListener > & /*xListener*/)
        throw( beans::UnknownPropertyException, lang::WrappedTargetException, uno::RuntimeException, std::exception )
{
    OSL_FAIL("not implemented");
}

void SwXFootnoteProperties::removeVetoableChangeListener(
    const OUString& /*rPropertyName*/, const uno::Reference< beans::XVetoableChangeListener > & /*xListener*/)
        throw( beans::UnknownPropertyException, lang::WrappedTargetException, uno::RuntimeException, std::exception )
{
    OSL_FAIL("not implemented");
}

OUString SwXEndnoteProperties::getImplementationName(void) throw( RuntimeException, std::exception )
{
    return OUString("SwXEndnoteProperties");
}

sal_Bool SwXEndnoteProperties::supportsService(const OUString& rServiceName) throw( RuntimeException, std::exception )
{
    return cppu::supportsService(this, rServiceName);
}

Sequence< OUString > SwXEndnoteProperties::getSupportedServiceNames(void) throw( RuntimeException, std::exception )
{
    Sequence< OUString > aRet(1);
    OUString* pArray = aRet.getArray();
    pArray[0] = "com.sun.star.text.FootnoteSettings";
    return aRet;
}

SwXEndnoteProperties::SwXEndnoteProperties(SwDoc* pDc) :
    pDoc(pDc),
    m_pPropertySet(GetEndnoteSet())
{
}

SwXEndnoteProperties::~SwXEndnoteProperties()
{
}

uno::Reference< beans::XPropertySetInfo >  SwXEndnoteProperties::getPropertySetInfo(void) throw( uno::RuntimeException, std::exception )
{
    static uno::Reference< beans::XPropertySetInfo >  aRef = m_pPropertySet->getPropertySetInfo();
    return aRef;
}

void SwXEndnoteProperties::setPropertyValue(const OUString& rPropertyName, const uno::Any& aValue)
    throw( beans::UnknownPropertyException, beans::PropertyVetoException, lang::IllegalArgumentException,
        lang::WrappedTargetException, uno::RuntimeException, std::exception )
{
    SolarMutexGuard aGuard;
    if(pDoc)
    {
        const SfxItemPropertySimpleEntry*  pEntry = m_pPropertySet->getPropertyMap().getByName( rPropertyName );
        if(pEntry)
        {
            if ( pEntry->nFlags & PropertyAttribute::READONLY)
                throw PropertyVetoException("Property is read-only: " + rPropertyName, static_cast < cppu::OWeakObject * > ( this ) );
            SwEndNoteInfo aEndInfo(pDoc->GetEndNoteInfo());
            switch(pEntry->nWID)
            {
                case WID_PREFIX:
                {
                    OUString uTmp;
                    aValue >>= uTmp;
                    aEndInfo.SetPrefix(uTmp);
                }
                break;
                case WID_SUFFIX:
                {
                    OUString uTmp;
                    aValue >>= uTmp;
                    aEndInfo.SetSuffix(uTmp);
                }
                break;
                case  WID_NUMBERING_TYPE :
                {
                    sal_Int16 nTmp = 0;
                    aValue >>= nTmp;
                    aEndInfo.aFmt.SetNumberingType(nTmp);
                }
                break;
                case  WID_START_AT:
                {
                    sal_Int16 nTmp = 0;
                    aValue >>= nTmp;
                    aEndInfo.nFtnOffset = nTmp;
                }
                break;
                case  WID_PARAGRAPH_STYLE    :
                {
                    SwTxtFmtColl* pColl = lcl_GetParaStyle(pDoc, aValue);
                    if(pColl)
                        aEndInfo.SetFtnTxtColl(*pColl);
                }
                break;
                case  WID_PAGE_STYLE :
                {
                    SwPageDesc* pDesc = lcl_GetPageDesc(pDoc, aValue);
                    if(pDesc)
                        aEndInfo.ChgPageDesc( pDesc );
                }
                break;
                case WID_ANCHOR_CHARACTER_STYLE:
                case  WID_CHARACTER_STYLE    :
                {
                    SwCharFmt* pFmt = lcl_getCharFmt(pDoc, aValue);
                    if(pFmt)
                    {
                        if(pEntry->nWID == WID_ANCHOR_CHARACTER_STYLE)
                            aEndInfo.SetAnchorCharFmt(pFmt);
                        else
                            aEndInfo.SetCharFmt(pFmt);
                    }
                }
                break;
            }
            pDoc->SetEndNoteInfo(aEndInfo);
        }
        else
            throw UnknownPropertyException("Unknown property: " + rPropertyName, static_cast < cppu::OWeakObject * > ( this ) );
    }
}

uno::Any SwXEndnoteProperties::getPropertyValue(const OUString& rPropertyName)
    throw( UnknownPropertyException, WrappedTargetException, RuntimeException, std::exception )
{
    SolarMutexGuard aGuard;
    uno::Any aRet;
    if(pDoc)
    {
        const SfxItemPropertySimpleEntry*  pEntry = m_pPropertySet->getPropertyMap().getByName( rPropertyName );
        if(pEntry)
        {
            const SwEndNoteInfo& rEndInfo = pDoc->GetEndNoteInfo();
            switch(pEntry->nWID)
            {
                case WID_PREFIX:
                    aRet <<= rEndInfo.GetPrefix();
                break;
                case WID_SUFFIX:
                    aRet <<= rEndInfo.GetSuffix();
                break;
                case  WID_NUMBERING_TYPE :
                    aRet <<= rEndInfo.aFmt.GetNumberingType();
                break;
                case  WID_START_AT:
                    aRet <<= (sal_Int16)rEndInfo.nFtnOffset;
                break;
                case  WID_PARAGRAPH_STYLE    :
                {
                    SwTxtFmtColl* pColl = rEndInfo.GetFtnTxtColl();
                    OUString aString;
                    if(pColl)
                        aString = pColl->GetName();
                    SwStyleNameMapper::FillProgName(
                            aString,
                            aString,
                            nsSwGetPoolIdFromName::GET_POOLID_TXTCOLL,
                            true);
                    aRet <<= aString;

                }
                break;
                case  WID_PAGE_STYLE :
                {
                    OUString aString;
                    if( rEndInfo.KnowsPageDesc() )
                    {
                        SwStyleNameMapper::FillProgName(
                            rEndInfo.GetPageDesc( *pDoc )->GetName(),
                            aString,
                            nsSwGetPoolIdFromName::GET_POOLID_PAGEDESC,
                            true );
                    }
                    aRet <<= aString;
                }
                break;
                case WID_ANCHOR_CHARACTER_STYLE:
                case WID_CHARACTER_STYLE:
                {
                    OUString aString;
                    const SwCharFmt* pCharFmt = 0;
                    if( pEntry->nWID == WID_ANCHOR_CHARACTER_STYLE )
                    {
                        if( rEndInfo.GetAnchorCharFmtDep()->GetRegisteredIn() )
                            pCharFmt = rEndInfo.GetAnchorCharFmt(*pDoc);
                    }
                    else
                    {
                        if( rEndInfo.GetCharFmtDep()->GetRegisteredIn() )
                            pCharFmt = rEndInfo.GetCharFmt(*pDoc);
                    }
                    if( pCharFmt )
                    {
                        SwStyleNameMapper::FillProgName(
                                pCharFmt->GetName(),
                                aString,
                                nsSwGetPoolIdFromName::GET_POOLID_CHRFMT,
                                true );
                    }
                    aRet <<= aString;
                }
                break;
            }
        }
        else
            throw UnknownPropertyException("Unknown property: " + rPropertyName, static_cast < cppu::OWeakObject * > ( this ) );
    }
    return aRet;
}

void SwXEndnoteProperties::addPropertyChangeListener(
    const OUString& /*PropertyName*/, const uno::Reference< beans::XPropertyChangeListener > & /*xListener*/) throw( beans::UnknownPropertyException, lang::WrappedTargetException, uno::RuntimeException, std::exception )
{
    OSL_FAIL("not implemented");
}

void SwXEndnoteProperties::removePropertyChangeListener(const OUString& /*PropertyName*/,
        const uno:: Reference< beans::XPropertyChangeListener > & /*xListener*/)
        throw( beans::UnknownPropertyException, lang::WrappedTargetException, uno::RuntimeException, std::exception )
{
    OSL_FAIL("not implemented");
}

void SwXEndnoteProperties::addVetoableChangeListener(const OUString& /*PropertyName*/,
    const uno:: Reference< beans::XVetoableChangeListener > & /*xListener*/)
    throw( beans::UnknownPropertyException, lang::WrappedTargetException, uno::RuntimeException, std::exception )
{
    OSL_FAIL("not implemented");
}

void SwXEndnoteProperties::removeVetoableChangeListener(const OUString& /*PropertyName*/, const uno:: Reference< beans::XVetoableChangeListener > & /*xListener*/)
    throw( beans::UnknownPropertyException, lang::WrappedTargetException, uno::RuntimeException, std::exception )
{
    OSL_FAIL("not implemented");
}

OUString SwXLineNumberingProperties::getImplementationName(void) throw( RuntimeException, std::exception )
{
    return OUString("SwXLineNumberingProperties");
}

sal_Bool SwXLineNumberingProperties::supportsService(const OUString& rServiceName) throw( RuntimeException, std::exception )
{
    return cppu::supportsService(this, rServiceName);
}

Sequence< OUString > SwXLineNumberingProperties::getSupportedServiceNames(void) throw( RuntimeException, std::exception )
{
    Sequence< OUString > aRet(1);
    OUString* pArray = aRet.getArray();
    pArray[0] = "com.sun.star.text.LineNumberingProperties";
    return aRet;
}

SwXLineNumberingProperties::SwXLineNumberingProperties(SwDoc* pDc) :
    pDoc(pDc),
    m_pPropertySet(GetLineNumberingSet())
{
}

SwXLineNumberingProperties::~SwXLineNumberingProperties()
{
}

uno::Reference< beans::XPropertySetInfo >  SwXLineNumberingProperties::getPropertySetInfo(void) throw( uno::RuntimeException, std::exception )
{
    static uno::Reference< beans::XPropertySetInfo >  aRef = m_pPropertySet->getPropertySetInfo();
    return aRef;
}

void SwXLineNumberingProperties::setPropertyValue(
    const OUString& rPropertyName, const Any& aValue)
        throw( UnknownPropertyException, PropertyVetoException,
                IllegalArgumentException, WrappedTargetException, RuntimeException, std::exception )
{
    SolarMutexGuard aGuard;
    if(pDoc)
    {
        const SfxItemPropertySimpleEntry*  pEntry = m_pPropertySet->getPropertyMap().getByName( rPropertyName );
        if(pEntry)
        {
            if ( pEntry->nFlags & PropertyAttribute::READONLY)
                throw PropertyVetoException("Property is read-only: " + rPropertyName, static_cast < cppu::OWeakObject * > ( this ) );
            SwLineNumberInfo  aInfo(pDoc->GetLineNumberInfo());
            switch(pEntry->nWID)
            {
                case WID_NUM_ON:
                {
                    bool bVal = *(sal_Bool*)aValue.getValue();
                    aInfo.SetPaintLineNumbers(bVal);
                }
                break;
                case WID_CHARACTER_STYLE :
                {
                    SwCharFmt* pFmt = lcl_getCharFmt(pDoc, aValue);
                    if(pFmt)
                        aInfo.SetCharFmt(pFmt);
                }
                break;
                case WID_NUMBERING_TYPE  :
                {
                    SvxNumberType aNumType(aInfo.GetNumType());
                    sal_Int16 nTmp = 0;
                    aValue >>= nTmp;
                    aNumType.SetNumberingType(nTmp);
                    aInfo.SetNumType(aNumType);
                }
                break;
                case WID_NUMBER_POSITION :
                {
                    sal_Int16 nTmp = 0;
                    aValue >>= nTmp;
                    switch(nTmp)
                    {
                        case  style::LineNumberPosition::LEFT:
                             aInfo.SetPos(LINENUMBER_POS_LEFT); ;
                        break;
                        case style::LineNumberPosition::RIGHT :
                             aInfo.SetPos(LINENUMBER_POS_RIGHT);       ;
                        break;
                        case  style::LineNumberPosition::INSIDE:
                            aInfo.SetPos(LINENUMBER_POS_INSIDE);      ;
                        break;
                        case  style::LineNumberPosition::OUTSIDE:
                            aInfo.SetPos(LINENUMBER_POS_OUTSIDE);
                        break;
                    }
                }
                break;
                case WID_DISTANCE        :
                {
                    sal_Int32 nVal = 0;
                    aValue >>= nVal;
                    sal_Int32 nTmp = convertMm100ToTwip(nVal);
                    if (nTmp > USHRT_MAX)
                        nTmp = USHRT_MAX;
                    aInfo.SetPosFromLeft( static_cast< sal_uInt16 >(nTmp) );
                }
                break;
                case WID_INTERVAL   :
                {
                    sal_Int16 nTmp = 0;
                    aValue >>= nTmp;
                    if( nTmp > 0)
                        aInfo.SetCountBy(nTmp);
                }
                break;
                case WID_SEPARATOR_TEXT  :
                {
                    OUString uTmp;
                    aValue >>= uTmp;
                    aInfo.SetDivider(uTmp);
                }
                break;
                case WID_SEPARATOR_INTERVAL:
                {
                    sal_Int16 nTmp = 0;
                    aValue >>= nTmp;
                    if( nTmp >= 0)
                        aInfo.SetDividerCountBy(nTmp);
                }
                break;
                case WID_COUNT_EMPTY_LINES :
                {
                    bool bVal = *(sal_Bool*)aValue.getValue();
                    aInfo.SetCountBlankLines(bVal);
                }
                break;
                case WID_COUNT_LINES_IN_FRAMES :
                {
                    bool bVal = *(sal_Bool*)aValue.getValue();
                    aInfo.SetCountInFlys(bVal);
                }
                break;
                case WID_RESTART_AT_EACH_PAGE :
                {
                    bool bVal = *(sal_Bool*)aValue.getValue();
                    aInfo.SetRestartEachPage(bVal);
                }
                break;
            }
            pDoc->SetLineNumberInfo(aInfo);
        }
        else
            throw UnknownPropertyException("Unknown property: " + rPropertyName, static_cast < cppu::OWeakObject * > ( this ) );
    }
    else
        throw uno::RuntimeException();
}

Any SwXLineNumberingProperties::getPropertyValue(const OUString& rPropertyName)
    throw( UnknownPropertyException, WrappedTargetException, RuntimeException, std::exception )
{
    SolarMutexGuard aGuard;
    Any aRet;
    if(pDoc)
    {
        const SfxItemPropertySimpleEntry*  pEntry = m_pPropertySet->getPropertyMap().getByName( rPropertyName );
        if(pEntry)
        {
            const SwLineNumberInfo& rInfo = pDoc->GetLineNumberInfo();
            switch(pEntry->nWID)
            {
                case WID_NUM_ON:
                {
                    sal_Bool bTemp = rInfo.IsPaintLineNumbers();
                    aRet.setValue(&bTemp, ::getCppuBooleanType());
                }
                break;
                case WID_CHARACTER_STYLE :
                {
                    OUString aString;
                    // return empty string if no char format is set
                    // otherwise it would be created here
                    if(rInfo.HasCharFormat())
                    {
                        SwStyleNameMapper::FillProgName(
                                    rInfo.GetCharFmt(*pDoc)->GetName(),
                                    aString,
                                    nsSwGetPoolIdFromName::GET_POOLID_CHRFMT,
                                    true);
                    }
                    aRet <<= aString;
                }
                break;
                case WID_NUMBERING_TYPE  :
                    aRet <<= rInfo.GetNumType().GetNumberingType();
                break;
                case WID_NUMBER_POSITION :
                {
                    sal_Int16 nRet = 0;
                    switch(rInfo.GetPos())
                    {
                        case  LINENUMBER_POS_LEFT:
                            nRet = style::LineNumberPosition::LEFT;
                        break;
                        case LINENUMBER_POS_RIGHT :
                            nRet = style::LineNumberPosition::RIGHT      ;
                        break;
                        case  LINENUMBER_POS_INSIDE:
                            nRet = style::LineNumberPosition::INSIDE     ;
                        break;
                        case LINENUMBER_POS_OUTSIDE :
                            nRet = style::LineNumberPosition::OUTSIDE    ;
                        break;
                    }
                    aRet <<= nRet;
                }
                break;
                case WID_DISTANCE        :
                {
                    sal_uInt32 nPos = rInfo.GetPosFromLeft();
                    if(USHRT_MAX == nPos)
                        nPos = 0;
                    aRet <<= static_cast < sal_Int32 >(convertTwipToMm100(nPos));
                }
                break;
                case WID_INTERVAL   :
                    aRet <<= (sal_Int16)rInfo.GetCountBy();
                break;
                case WID_SEPARATOR_TEXT  :
                    aRet <<= rInfo.GetDivider();
                break;
                case WID_SEPARATOR_INTERVAL:
                    aRet <<= (sal_Int16)rInfo.GetDividerCountBy();
                break;
                case WID_COUNT_EMPTY_LINES :
                {
                    sal_Bool bTemp = rInfo.IsCountBlankLines();
                    aRet.setValue(&bTemp, ::getCppuBooleanType());
                }
                break;
                case WID_COUNT_LINES_IN_FRAMES :
                {
                    sal_Bool bTemp = rInfo.IsCountInFlys();
                    aRet.setValue(&bTemp, ::getCppuBooleanType());
                }
                break;
                case WID_RESTART_AT_EACH_PAGE :
                {
                    sal_Bool bTemp = rInfo.IsRestartEachPage();
                    aRet.setValue(&bTemp, ::getCppuBooleanType());
                }
                break;
            }
        }
        else
            throw UnknownPropertyException("Unknown property: " + rPropertyName, static_cast < cppu::OWeakObject * > ( this ) );
    }
    else
        throw uno::RuntimeException();
    return aRet;
}

void SwXLineNumberingProperties::addPropertyChangeListener(const OUString& /*rPropertyName*/, const uno:: Reference< beans::XPropertyChangeListener > & /*xListener*/) throw( beans::UnknownPropertyException, lang::WrappedTargetException, uno::RuntimeException, std::exception )
{
OSL_FAIL("not implemented");
}

void SwXLineNumberingProperties::removePropertyChangeListener(const OUString& /*rPropertyName*/, const uno:: Reference< beans::XPropertyChangeListener > & /*xListener*/) throw( beans::UnknownPropertyException, lang::WrappedTargetException, uno::RuntimeException, std::exception )
{
OSL_FAIL("not implemented");
}

void SwXLineNumberingProperties::addVetoableChangeListener(const OUString& /*rPropertyName*/, const uno:: Reference< beans::XVetoableChangeListener > & /*xListener*/) throw( beans::UnknownPropertyException, lang::WrappedTargetException, uno::RuntimeException, std::exception )
{
OSL_FAIL("not implemented");
}

void SwXLineNumberingProperties::removeVetoableChangeListener(const OUString& /*rPropertyName*/, const uno:: Reference< beans::XVetoableChangeListener > & /*xListener*/)
    throw( beans::UnknownPropertyException, lang::WrappedTargetException, uno::RuntimeException, std::exception )
{
OSL_FAIL("not implemented");
}

const char aInvalidStyle[] = "__XXX___invalid";

bool SwXNumberingRules::isInvalidStyle(const OUString &rName)
{
    return rName == aInvalidStyle;
}

namespace
{
    class theSwXNumberingRulesUnoTunnelId : public rtl::Static< UnoTunnelIdInit, theSwXNumberingRulesUnoTunnelId > {};
}

const uno::Sequence< sal_Int8 > & SwXNumberingRules::getUnoTunnelId()
{
    return theSwXNumberingRulesUnoTunnelId::get().getSeq();
}

// return implementation specific data
sal_Int64 SwXNumberingRules::getSomething( const uno::Sequence< sal_Int8 > & rId ) throw(uno::RuntimeException, std::exception)
{
    if( rId.getLength() == 16
        && 0 == memcmp( getUnoTunnelId().getConstArray(),
                                        rId.getConstArray(), 16 ) )
    {
        return sal::static_int_cast< sal_Int64 >( reinterpret_cast< sal_IntPtr >(this) );
    }
    return 0;
}

OUString SwXNumberingRules::getImplementationName(void) throw( RuntimeException, std::exception )
{
    return OUString("SwXNumberingRules");
}

sal_Bool SwXNumberingRules::supportsService(const OUString& rServiceName) throw( RuntimeException, std::exception )
{
    return cppu::supportsService(this, rServiceName);
}

Sequence< OUString > SwXNumberingRules::getSupportedServiceNames(void) throw( RuntimeException, std::exception )
{
    Sequence< OUString > aRet(1);
    OUString* pArray = aRet.getArray();
    pArray[0] = "com.sun.star.text.NumberingRules";
    return aRet;
}

SwXNumberingRules::SwXNumberingRules(const SwNumRule& rRule, SwDoc* doc) :
    pDoc(doc),
    pDocShell(0),
    pNumRule(new SwNumRule(rRule)),
    m_pPropertySet(GetNumberingRulesSet()),
    bOwnNumRuleCreated(true)
{
    // first organize the document - it is dependent on the set character formats
    // if no format is set, it should work as well
    for( sal_uInt16 i = 0; i < MAXLEVEL; ++i)
    {
        SwNumFmt rFmt(pNumRule->Get(i));
        SwCharFmt* pCharFmt = rFmt.GetCharFmt();
        if(pCharFmt)
        {
            pDoc = pCharFmt->GetDoc();
            break;
        }
    }
    if(pDoc)
        pDoc->GetPageDescFromPool(RES_POOLPAGE_STANDARD)->Add(this);
    for(sal_uInt16 i = 0; i < MAXLEVEL; ++i)
    {
        m_sNewCharStyleNames[i] = aInvalidStyle;
        m_sNewBulletFontNames[i] = aInvalidStyle;
    }
}

SwXNumberingRules::SwXNumberingRules(SwDocShell& rDocSh) :
    pDoc(0),
    pDocShell(&rDocSh),
    pNumRule(0),
    m_pPropertySet(GetNumberingRulesSet()),
    bOwnNumRuleCreated(false)
{
    pDocShell->GetDoc()->GetPageDescFromPool(RES_POOLPAGE_STANDARD)->Add(this);
}

SwXNumberingRules::SwXNumberingRules(SwDoc& rDoc) :
    pDoc(&rDoc),
    pDocShell(0),
    pNumRule(0),
    m_pPropertySet(GetNumberingRulesSet()),
    bOwnNumRuleCreated(false)
{
    rDoc.GetPageDescFromPool(RES_POOLPAGE_STANDARD)->Add(this);
    m_sCreatedNumRuleName = rDoc.GetUniqueNumRuleName();
#if OSL_DEBUG_LEVEL > 1
    const sal_uInt16 nIndex =
#endif
    rDoc.MakeNumRule( m_sCreatedNumRuleName, 0, false,
                      // #i89178#
                      numfunc::GetDefaultPositionAndSpaceMode() );
#if OSL_DEBUG_LEVEL > 1
    (void)nIndex;
#endif
}

SwXNumberingRules::~SwXNumberingRules()
{
    SolarMutexGuard aGuard;
    if(pDoc && !m_sCreatedNumRuleName.isEmpty())
        pDoc->DelNumRule( m_sCreatedNumRuleName );
    if( bOwnNumRuleCreated )
        delete pNumRule;
}

void SwXNumberingRules::replaceByIndex(sal_Int32 nIndex, const uno::Any& rElement)
    throw( lang::IllegalArgumentException, lang::IndexOutOfBoundsException,
                  lang::WrappedTargetException, uno::RuntimeException, std::exception)
{
    SolarMutexGuard aGuard;
    if(nIndex < 0 || MAXLEVEL <= nIndex)
        throw lang::IndexOutOfBoundsException();

    if(rElement.getValueType().getTypeClass() != uno::TypeClass_SEQUENCE)
        throw lang::IllegalArgumentException();
    const uno::Sequence<beans::PropertyValue>& rProperties =
                    *(const uno::Sequence<beans::PropertyValue>*)rElement.getValue();
    SwNumRule* pRule = 0;
    if(pNumRule)
        SwXNumberingRules::SetNumberingRuleByIndex( *pNumRule,
                            rProperties, nIndex);
    else if(pDocShell)
    {
        // #i87650# - correction of cws swwarnings:
        SwNumRule aNumRule( *(pDocShell->GetDoc()->GetOutlineNumRule()) );
        SwXNumberingRules::SetNumberingRuleByIndex( aNumRule,
                            rProperties, nIndex);
        // set character format if needed
        const SwCharFmts* pFmts = pDocShell->GetDoc()->GetCharFmts();
        const size_t nChCount = pFmts->size();
        for(sal_uInt16 i = 0; i < MAXLEVEL;i++)
        {
            SwNumFmt aFmt(aNumRule.Get( i ));
            if (!m_sNewCharStyleNames[i].isEmpty() &&
                m_sNewCharStyleNames[i] != UNO_NAME_CHARACTER_FORMAT_NONE &&
                (!aFmt.GetCharFmt() || aFmt.GetCharFmt()->GetName()!= m_sNewCharStyleNames[i]))
            {
                SwCharFmt* pCharFmt = 0;
                for(size_t j = 0; j< nChCount; ++j)
                {
                    SwCharFmt* pTmp = (*pFmts)[j];
                    if(pTmp->GetName() == m_sNewCharStyleNames[i])
                    {
                        pCharFmt = pTmp;
                        break;
                    }
                }
                if(!pCharFmt)
                {
                    SfxStyleSheetBase* pBase;
                    pBase = pDocShell->GetStyleSheetPool()->Find(m_sNewCharStyleNames[i],
                                                                    SFX_STYLE_FAMILY_CHAR);
                    if(!pBase)
                        pBase = &pDocShell->GetStyleSheetPool()->Make(m_sNewCharStyleNames[i], SFX_STYLE_FAMILY_CHAR);
                    pCharFmt = ((SwDocStyleSheet*)pBase)->GetCharFmt();

                }
                aFmt.SetCharFmt( pCharFmt );
                aNumRule.Set( i, aFmt );
            }
        }
        pDocShell->GetDoc()->SetOutlineNumRule( aNumRule );
    }
    else if(!pNumRule && pDoc && !m_sCreatedNumRuleName.isEmpty() &&
        0 != (pRule = pDoc->FindNumRulePtr( m_sCreatedNumRuleName )))
    {
        SwXNumberingRules::SetNumberingRuleByIndex( *pRule,
                            rProperties, nIndex);

        pRule->Validate();
    }
    else
        throw uno::RuntimeException();
}

sal_Int32 SwXNumberingRules::getCount(void) throw( uno::RuntimeException, std::exception )
{
    return MAXLEVEL;
}

uno::Any SwXNumberingRules::getByIndex(sal_Int32 nIndex)
    throw( lang::IndexOutOfBoundsException, lang::WrappedTargetException,
            uno::RuntimeException, std::exception )
{
    SolarMutexGuard aGuard;
    if(nIndex < 0 || MAXLEVEL <= nIndex)
        throw lang::IndexOutOfBoundsException();

    uno::Any aVal;
    const SwNumRule* pRule = pNumRule;
    if(!pRule && pDoc && !m_sCreatedNumRuleName.isEmpty())
        pRule = pDoc->FindNumRulePtr( m_sCreatedNumRuleName );
    if(pRule)
    {
        uno::Sequence<beans::PropertyValue> aRet = GetNumberingRuleByIndex(
                                        *pRule, nIndex);
        aVal.setValue(&aRet, ::getCppuType((uno::Sequence<beans::PropertyValue>*)0));

    }
    else if(pDocShell)
    {
        uno::Sequence<beans::PropertyValue> aRet = GetNumberingRuleByIndex(
                *pDocShell->GetDoc()->GetOutlineNumRule(), nIndex);
        aVal.setValue(&aRet, ::getCppuType((uno::Sequence<beans::PropertyValue>*)0));
    }
    else
        throw uno::RuntimeException();
    return aVal;
}

uno::Type SwXNumberingRules::getElementType(void)
    throw( uno::RuntimeException, std::exception )
{
    return ::getCppuType((uno::Sequence<beans::PropertyValue>*)0);
}

sal_Bool SwXNumberingRules::hasElements(void) throw( uno::RuntimeException, std::exception )
{
    return sal_True;
}

uno::Sequence<beans::PropertyValue> SwXNumberingRules::GetNumberingRuleByIndex(
                const SwNumRule& rNumRule, sal_Int32 nIndex) const
{
    SolarMutexGuard aGuard;
    OSL_ENSURE( 0 <= nIndex && nIndex < MAXLEVEL, "index out of range" );

    const SwNumFmt& rFmt = rNumRule.Get( (sal_uInt16)nIndex );

    bool bChapterNum = pDocShell != 0;

    PropValDataArr  aPropertyValues;
    //fill all properties into the array

    //adjust
    SvxAdjust eAdj = rFmt.GetNumAdjust();
    sal_Int16 nINT16 = aSvxToUnoAdjust[eAdj];
    PropValData* pData = new PropValData((void*)&nINT16, "Adjust", ::cppu::UnoType<sal_Int16>::get() );
    aPropertyValues.push_back(pData);

    //parentnumbering
    nINT16 = rFmt.GetIncludeUpperLevels();
    pData = new PropValData((void*)&nINT16, "ParentNumbering", ::cppu::UnoType<sal_Int16>::get());
    aPropertyValues.push_back(pData);

    //prefix
    OUString aUString = rFmt.GetPrefix();
    pData = new PropValData((void*)&aUString, "Prefix", ::cppu::UnoType<OUString>::get());
    aPropertyValues.push_back(pData);

    //suffix
    aUString = rFmt.GetSuffix();
    pData = new PropValData((void*)&aUString, "Suffix", ::cppu::UnoType<OUString>::get());
    aPropertyValues.push_back(pData);

    //char style name
    SwCharFmt* pCharFmt = rFmt.GetCharFmt();
    OUString CharStyleName;
    if(pCharFmt)
        CharStyleName = pCharFmt->GetName();
    //egal ob ein Style vorhanden ist oder nicht ueberschreibt der Array-Eintrag diesen String
    if (!m_sNewCharStyleNames[nIndex].isEmpty() &&
        !SwXNumberingRules::isInvalidStyle(m_sNewCharStyleNames[nIndex]))
    {
        CharStyleName = m_sNewCharStyleNames[nIndex];
    }

    aUString = OUString();
    SwStyleNameMapper::FillProgName( CharStyleName, aUString, nsSwGetPoolIdFromName::GET_POOLID_CHRFMT, true );
    pData = new PropValData((void*)&aUString, "CharStyleName", ::cppu::UnoType<OUString>::get());
    aPropertyValues.push_back(pData);

    //startvalue
    nINT16 = rFmt.GetStart();
    pData = new PropValData((void*)&nINT16, "StartWith", ::cppu::UnoType<sal_Int16>::get());
    aPropertyValues.push_back(pData);

    if ( rFmt.GetPositionAndSpaceMode() == SvxNumberFormat::LABEL_WIDTH_AND_POSITION )
    {
        //leftmargin
        sal_Int32 nINT32 = convertTwipToMm100(rFmt.GetAbsLSpace());
        pData = new PropValData((void*)&nINT32, UNO_NAME_LEFT_MARGIN, ::cppu::UnoType<sal_Int32>::get());
        aPropertyValues.push_back(pData);

        //chartextoffset
        nINT32 = convertTwipToMm100(rFmt.GetCharTextDistance());
        pData = new PropValData((void*)&nINT32, UNO_NAME_SYMBOL_TEXT_DISTANCE, ::cppu::UnoType<sal_Int32>::get());
        aPropertyValues.push_back(pData);

        //firstlineoffset
        nINT32 = convertTwipToMm100(rFmt.GetFirstLineOffset());
        pData = new PropValData((void*)&nINT32, UNO_NAME_FIRST_LINE_OFFSET, ::cppu::UnoType<sal_Int32>::get());
        aPropertyValues.push_back(pData);
    }

    // PositionAndSpaceMode
    nINT16 = PositionAndSpaceMode::LABEL_WIDTH_AND_POSITION;
    if ( rFmt.GetPositionAndSpaceMode() == SvxNumberFormat::LABEL_ALIGNMENT )
    {
        nINT16 = PositionAndSpaceMode::LABEL_ALIGNMENT;
    }
    pData = new PropValData( (void*)&nINT16,
                             UNO_NAME_POSITION_AND_SPACE_MODE,
                             ::cppu::UnoType<sal_Int16>::get() );
    aPropertyValues.push_back(pData);

    if ( rFmt.GetPositionAndSpaceMode() == SvxNumberFormat::LABEL_ALIGNMENT )
    {
        // LabelFollowedBy
        nINT16 = LabelFollow::LISTTAB;
        if ( rFmt.GetLabelFollowedBy() == SvxNumberFormat::SPACE )
        {
            nINT16 = LabelFollow::SPACE;
        }
        else if ( rFmt.GetLabelFollowedBy() == SvxNumberFormat::NOTHING )
        {
            nINT16 = LabelFollow::NOTHING;
        }
        pData = new PropValData( (void*)&nINT16,
                                 UNO_NAME_LABEL_FOLLOWED_BY,
                                 ::cppu::UnoType<sal_Int16>::get() );
        aPropertyValues.push_back(pData);

        // ListtabStopPosition
        sal_Int32 nINT32 = convertTwipToMm100(rFmt.GetListtabPos());
        pData = new PropValData( (void*)&nINT32,
                                 UNO_NAME_LISTTAB_STOP_POSITION,
                                 ::cppu::UnoType<sal_Int32>::get());
        aPropertyValues.push_back(pData);

        // FirstLineIndent
        nINT32 = convertTwipToMm100(rFmt.GetFirstLineIndent());
        pData = new PropValData( (void*)&nINT32,
                                 UNO_NAME_FIRST_LINE_INDENT,
                                 ::cppu::UnoType<sal_Int32>::get());
        aPropertyValues.push_back(pData);

        // IndentAt
        nINT32 = convertTwipToMm100(rFmt.GetIndentAt());
        pData = new PropValData( (void*)&nINT32,
                                 UNO_NAME_INDENT_AT,
                                 ::cppu::UnoType<sal_Int32>::get());
        aPropertyValues.push_back(pData);
    }

    //numberingtype
    nINT16 = rFmt.GetNumberingType();
    pData = new PropValData((void*)&nINT16, "NumberingType", ::cppu::UnoType<sal_Int16>::get());
    aPropertyValues.push_back(pData);

    if(!bChapterNum)
    {
        if(SVX_NUM_CHAR_SPECIAL == rFmt.GetNumberingType())
        {
            //BulletId
            nINT16 = rFmt.GetBulletChar();
            pData = new PropValData((void*)&nINT16, "BulletId", ::cppu::UnoType<sal_Int16>::get());
            aPropertyValues.push_back(pData);

            const Font* pFont = rFmt.GetBulletFont();

            //BulletChar
            aUString = OUString(rFmt.GetBulletChar());
            pData = new PropValData((void*)&aUString, "BulletChar", ::cppu::UnoType<OUString>::get());
            aPropertyValues.push_back(pData);

            //BulletFontName
            aUString = pFont ? pFont->GetStyleName() : OUString();
            pData = new PropValData((void*)&aUString, "BulletFontName", ::cppu::UnoType<OUString>::get());
            aPropertyValues.push_back(pData);

            //BulletFont
            if(pFont)
            {
                 awt::FontDescriptor aDesc;
                SvxUnoFontDescriptor::ConvertFromFont( *pFont, aDesc );
                pData = new PropValData((void*)&aDesc, UNO_NAME_BULLET_FONT, cppu::UnoType<awt::FontDescriptor>::get());
                aPropertyValues.push_back(pData);
            }
        }
        if(SVX_NUM_BITMAP == rFmt.GetNumberingType())
        {
            //GraphicURL
            const SvxBrushItem* pBrush = rFmt.GetBrush();
            if(pBrush)
            {
                Any aAny;
                pBrush->QueryValue( aAny, MID_GRAPHIC_URL );
                aAny >>= aUString;
            }
            else
                aUString = OUString();
            pData = new PropValData((void*)&aUString, UNO_NAME_GRAPHIC_URL, ::cppu::UnoType<OUString>::get());
            aPropertyValues.push_back(pData);

            //graphicbitmap
            const Graphic* pGraphic = 0;
            if(pBrush )
                pGraphic = pBrush->GetGraphic();
            if(pGraphic)
            {
                uno::Reference<awt::XBitmap> xBmp = VCLUnoHelper::CreateBitmap( pGraphic->GetBitmapEx() );
                pData = new PropValData((void*)&xBmp, UNO_NAME_GRAPHIC_BITMAP,
                                cppu::UnoType<awt::XBitmap>::get());
                aPropertyValues.push_back(pData);
            }
             Size aSize = rFmt.GetGraphicSize();
            // #i101131#
            // adjust conversion due to type mismatch between <Size> and <awt::Size>
            awt::Size aAwtSize(convertTwipToMm100(aSize.Width()), convertTwipToMm100(aSize.Height()));
            pData = new PropValData((void*)&aAwtSize, UNO_NAME_GRAPHIC_SIZE, ::cppu::UnoType<awt::Size>::get());
            aPropertyValues.push_back(pData);

            const SwFmtVertOrient* pOrient = rFmt.GetGraphicOrientation();
            if(pOrient)
            {
                pData = new PropValData((void*)0, UNO_NAME_VERT_ORIENT, ::cppu::UnoType<sal_Int16>::get());
                ((const SfxPoolItem*)pOrient)->QueryValue(pData->aVal, MID_VERTORIENT_ORIENT);
                aPropertyValues.push_back(pData);
            }
        }
    }
    else
    {
        // template name
        OUString sValue(SW_RES(STR_POOLCOLL_HEADLINE1 + nIndex));
        const SwTxtFmtColls* pColls = pDocShell->GetDoc()->GetTxtFmtColls();
        const size_t nCount = pColls->size();
        for(size_t i = 0; i < nCount; ++i)
        {
            SwTxtFmtColl &rTxtColl = *pColls->operator[](i);
            if(rTxtColl.IsDefault())
                continue;

            const sal_Int16 nOutLevel = rTxtColl.IsAssignedToListLevelOfOutlineStyle()
                                        ? static_cast<sal_Int16>(rTxtColl.GetAssignedOutlineStyleLevel())
                                        : MAXLEVEL;
            if ( nOutLevel == nIndex )
            {
                sValue = rTxtColl.GetName();
                break; // the style for the level in question has been found
            }
            else if( sValue==rTxtColl.GetName() )
            {
                // if the default for the level is existing, but its
                // level is different, then it cannot be the default.
                sValue = OUString();
            }
        }
        aUString = OUString();
        SwStyleNameMapper::FillProgName(sValue, aUString, nsSwGetPoolIdFromName::GET_POOLID_TXTCOLL, true);

        pData = new PropValData((void*)&aUString, UNO_NAME_HEADING_STYLE_NAME, ::cppu::UnoType<OUString>::get());
        aPropertyValues.push_back(pData);
    }

    uno::Sequence<beans::PropertyValue> aSeq(aPropertyValues.size());
    beans::PropertyValue* pArray = aSeq.getArray();

    for(size_t i = 0; i < aPropertyValues.size(); ++i)
    {
        pData = aPropertyValues[i];
        pArray[i].Value = pData->aVal;
        pArray[i].Name = pData->sPropName;
        pArray[i].Handle = -1;
    }
    for (PropValDataArr::const_iterator it = aPropertyValues.begin(); it != aPropertyValues.end(); ++it)
      delete *it;
    aPropertyValues.clear();
    return aSeq;
}

static PropValData* lcl_FindProperty(const char* cName, PropValDataArr&    rPropertyValues)
{
    const OUString sCmp = OUString::createFromAscii(cName);
    for(size_t i = 0; i < rPropertyValues.size(); ++i)
    {
        PropValData* pTemp = rPropertyValues[i];
        if(sCmp == pTemp->sPropName)
            return pTemp;
    }
    return 0;
}

void SwXNumberingRules::SetNumberingRuleByIndex(
            SwNumRule& rNumRule,
            const uno::Sequence<beans::PropertyValue>& rProperties, sal_Int32 nIndex)
    throw( uno::RuntimeException, lang::IllegalArgumentException )
{
    SolarMutexGuard aGuard;
    OSL_ENSURE( 0 <= nIndex && nIndex < MAXLEVEL, "index out of range" );

    // the order of the names is important!
    static const char* aNumPropertyNames[] =
    {
        "Adjust",                               //0
        "ParentNumbering",                      //1
        "Prefix",                               //2
        "Suffix",                               //3
        "CharStyleName",                        //4
        "StartWith",                            //5
        UNO_NAME_LEFT_MARGIN,                   //6
        UNO_NAME_SYMBOL_TEXT_DISTANCE,          //7
        UNO_NAME_FIRST_LINE_OFFSET,             //8
        UNO_NAME_POSITION_AND_SPACE_MODE, //9
        UNO_NAME_LABEL_FOLLOWED_BY,       //10
        UNO_NAME_LISTTAB_STOP_POSITION,   //11
        UNO_NAME_FIRST_LINE_INDENT,       //12
        UNO_NAME_INDENT_AT,               //13
        "NumberingType",                        //14
        UNO_NAME_PARAGRAPH_STYLE_NAME, //15
        // these are not in chapter numbering
        "BulletId",                             //16
        UNO_NAME_BULLET_FONT, //17
        "BulletFontName",                       //18
        "BulletChar",                           //19
        UNO_NAME_GRAPHIC_URL,    //20
        UNO_NAME_GRAPHIC_BITMAP, //21
        UNO_NAME_GRAPHIC_SIZE,   //22
        UNO_NAME_VERT_ORIENT,    //23
        // these are only in chapter numbering
        UNO_NAME_HEADING_STYLE_NAME, //24
        // these two are accepted but ignored for some reason
        "BulletRelSize",                         // 25
        "BulletColor"                            // 26
    };

    enum {
        NotInChapterFirst = 16,
        NotInChapterLast = 23,
        InChapterFirst = 24,
        InChapterLast = 24
    };

    const beans::PropertyValue* pPropArray = rProperties.getConstArray();
    PropValDataArr aPropertyValues;
    bool bExcept = false;
    for(sal_Int32 i = 0; i < rProperties.getLength() && !bExcept; i++)
    {
        const beans::PropertyValue& rProp = pPropArray[i];
        bExcept = true;
        for(size_t j = 0; j < SAL_N_ELEMENTS( aNumPropertyNames ); j++)
        {
            if (pDocShell &&
                j >= static_cast<size_t>(NotInChapterFirst) &&
                j <= static_cast<size_t>(NotInChapterLast))
                continue;
            if (!pDocShell &&
                j >= static_cast<size_t>(InChapterFirst) &&
                j <= static_cast<size_t>(InChapterLast))
                continue;
            if (rProp.Name.equalsAscii(aNumPropertyNames[j]))
            {
                bExcept = false;
                break;
            }
        }
        SAL_WARN_IF( bExcept, "sw.uno", "Unknown/incorrect property " << rProp.Name << ", failing" );
        PropValData* pData = new PropValData(rProp.Value, rProp.Name );
        aPropertyValues.push_back(pData);
    }

    SwNumFmt aFmt(rNumRule.Get( (sal_uInt16)nIndex ));
    bool bWrongArg = false;
    if(!bExcept)
       {
        SvxBrushItem* pSetBrush = 0;
        Size* pSetSize = 0;
        SwFmtVertOrient* pSetVOrient = 0;
        bool bCharStyleNameSet = false;

        for(size_t i = 0; i < SAL_N_ELEMENTS( aNumPropertyNames ) && !bExcept && !bWrongArg; ++i)
        {
            PropValData* pData = lcl_FindProperty(aNumPropertyNames[i], aPropertyValues);
            if(!pData)
                continue;
            switch(i)
            {
                case 0: //"Adjust"
                {
                    sal_Int16 nValue = 0;
                    pData->aVal >>= nValue;
                    if(nValue > 0 &&
                        nValue <= text::HoriOrientation::LEFT &&
                            USHRT_MAX != aUnoToSvxAdjust[nValue])
                    {
                        aFmt.SetNumAdjust((SvxAdjust)aUnoToSvxAdjust[nValue]);
                    }
                    else
                        bWrongArg = true;
                }
                break;
                case 1: //"ParentNumbering",
                {
                    sal_Int16 nSet = 0;
                    pData->aVal >>= nSet;
                    if(nSet >= 0 && MAXLEVEL >= nSet)
                        aFmt.SetIncludeUpperLevels( static_cast< sal_uInt8 >(nSet) );
                }
                break;
                case 2: //"Prefix",
                {
                    OUString uTmp;
                    pData->aVal >>= uTmp;
                    aFmt.SetPrefix(uTmp);
                }
                break;
                case 3: //"Suffix",
                {
                    OUString uTmp;
                    pData->aVal >>= uTmp;
                    aFmt.SetSuffix(uTmp);
                }
                break;
                case 4: //"CharStyleName",
                {
                    bCharStyleNameSet = true;
                    OUString uTmp;
                    pData->aVal >>= uTmp;
                    OUString sCharFmtName;
                    SwStyleNameMapper::FillUIName( uTmp, sCharFmtName, nsSwGetPoolIdFromName::GET_POOLID_CHRFMT, true );
                    if (sCharFmtName == UNO_NAME_CHARACTER_FORMAT_NONE)
                    {
                        m_sNewCharStyleNames[nIndex] = aInvalidStyle;
                        aFmt.SetCharFmt(0);
                    }
                    else if(pDocShell || pDoc)
                    {
                        SwDoc* pLocalDoc = pDoc ? pDoc : pDocShell->GetDoc();
                        const SwCharFmts* pFmts = pLocalDoc->GetCharFmts();
                        const size_t nChCount = pFmts->size();

                        SwCharFmt* pCharFmt = 0;
                        if (!sCharFmtName.isEmpty())
                        {
                            for(size_t j = 0; j< nChCount; ++j)
                            {
                                SwCharFmt* pTmp = (*pFmts)[j];
                                if(pTmp->GetName() == sCharFmtName)
                                {
                                    pCharFmt = pTmp;
                                    break;
                                }
                            }
                            if(!pCharFmt)
                            {

                                SfxStyleSheetBase* pBase;
                                SfxStyleSheetBasePool* pPool = pLocalDoc->GetDocShell()->GetStyleSheetPool();
                                pBase = ((SfxStyleSheetBasePool*)pPool)->Find(sCharFmtName, SFX_STYLE_FAMILY_CHAR);
                                if(!pBase)
                                    pBase = &pPool->Make(sCharFmtName, SFX_STYLE_FAMILY_CHAR);
                                pCharFmt = ((SwDocStyleSheet*)pBase)->GetCharFmt();
                            }
                        }
                        aFmt.SetCharFmt( pCharFmt );
                        // #i51842#
                        // If the character format has been found its name should not be in the
                        // char style names array
                        m_sNewCharStyleNames[nIndex] = OUString();
                     }
                    else
                        m_sNewCharStyleNames[nIndex] = sCharFmtName;
                }
                break;
                case 5: //"StartWith",
                {
                    sal_Int16 nVal = 0;
                    pData->aVal >>= nVal;
                    aFmt.SetStart(nVal);
                }
                break;
                case 6: //UNO_NAME_LEFT_MARGIN,
                {
                    sal_Int32 nValue = 0;
                    pData->aVal >>= nValue;
                    // #i23727# nValue can be negative
                    aFmt.SetAbsLSpace((short) convertMm100ToTwip(nValue));
                }
                break;
                case 7: //UNO_NAME_SYMBOL_TEXT_DISTANCE,
                {
                    sal_Int32 nValue = 0;
                    pData->aVal >>= nValue;
                    if(nValue >= 0)
                        aFmt.SetCharTextDistance((short) convertMm100ToTwip(nValue));
                    else
                        bWrongArg = true;
                }
                break;
                case 8: //UNO_NAME_FIRST_LINE_OFFSET,
                {
                    sal_Int32 nValue = 0;
                    pData->aVal >>= nValue;
                    // #i23727# nValue can be positive
                    nValue = convertMm100ToTwip(nValue);
                    aFmt.SetFirstLineOffset((short)nValue);
                }
                break;
                case 9: // UNO_NAME_POSITION_AND_SPACE_MODE
                {
                    sal_Int16 nValue = 0;
                    pData->aVal >>= nValue;
                    if ( nValue == 0 )
                    {
                        aFmt.SetPositionAndSpaceMode( SvxNumberFormat::LABEL_WIDTH_AND_POSITION );
                    }
                    else if ( nValue == 1 )
                    {
                        aFmt.SetPositionAndSpaceMode( SvxNumberFormat::LABEL_ALIGNMENT );
                    }
                    else
                    {
                        bWrongArg = true;
                    }
                }
                break;
                case 10: // UNO_NAME_LABEL_FOLLOWED_BY
                {
                    sal_Int16 nValue = 0;
                    pData->aVal >>= nValue;
                    if ( nValue == 0 )
                    {
                        aFmt.SetLabelFollowedBy( SvxNumberFormat::LISTTAB );
                    }
                    else if ( nValue == 1 )
                    {
                        aFmt.SetLabelFollowedBy( SvxNumberFormat::SPACE );
                    }
                    else if ( nValue == 2 )
                    {
                        aFmt.SetLabelFollowedBy( SvxNumberFormat::NOTHING );
                    }
                    else
                    {
                        bWrongArg = true;
                    }
                }
                break;
                case 11: // UNO_NAME_LISTTAB_STOP_POSITION
                {
                    sal_Int32 nValue = 0;
                    pData->aVal >>= nValue;
                    nValue = convertMm100ToTwip(nValue);
                    if ( nValue >= 0 )
                    {
                        aFmt.SetListtabPos( nValue );
                    }
                    else
                    {
                        bWrongArg = true;
                    }
                }
                break;
                case 12: // UNO_NAME_FIRST_LINE_INDENT
                {
                    sal_Int32 nValue = 0;
                    pData->aVal >>= nValue;
                    nValue = convertMm100ToTwip(nValue);
                    aFmt.SetFirstLineIndent( nValue );
                }
                break;
                case 13: // UNO_NAME_INDENT_AT
                {
                    sal_Int32 nValue = 0;
                    pData->aVal >>= nValue;
                    nValue = convertMm100ToTwip(nValue);
                    aFmt.SetIndentAt( nValue );
                }
                break;
                case 14: //"NumberingType"
                {
                    sal_Int16 nSet = 0;
                    pData->aVal >>= nSet;
                    if(nSet >= 0)
                        aFmt.SetNumberingType(nSet);
                    else
                        bWrongArg = true;
                }
                break;
                case 15: //"ParagraphStyleName"
                {
                    if( pDoc )
                    {
                        OUString uTmp;
                        pData->aVal >>= uTmp;
                        OUString sStyleName;
                        SwStyleNameMapper::FillUIName(uTmp, sStyleName, nsSwGetPoolIdFromName::GET_POOLID_TXTCOLL, true );
                        const SwTxtFmtColls* pColls = pDoc->GetTxtFmtColls();
                        const size_t nCount = pColls->size();
                        for(size_t k = 0; k < nCount; ++k)
                        {
                            SwTxtFmtColl &rTxtColl = *((*pColls)[k]);
                            if ( rTxtColl.GetName() == sStyleName )
                                rTxtColl.SetFmtAttr( SwNumRuleItem( rNumRule.GetName()));
                        }
                    }
                }
                break;
                case 16: //"BulletId",
                {
                    assert( !pDocShell );
                    sal_Int16 nSet = 0;
                    if( pData->aVal >>= nSet )
                        aFmt.SetBulletChar(nSet);
                    else
                        bWrongArg = true;
                }
                break;
                case 17: //UNO_NAME_BULLET_FONT,
                {
                    assert( !pDocShell );
                    awt::FontDescriptor* pDesc =  (awt::FontDescriptor*)pData->aVal.getValue();
                    if(pDesc)
                    {
                        // #i93725#
                        // do not accept "empty" font
                        if ( !pDesc->Name.isEmpty() )
                        {
                            Font aFont;
                            SvxUnoFontDescriptor::ConvertToFont( *pDesc, aFont );
                            aFmt.SetBulletFont(&aFont);
                        }
                    }
                    else
                        bWrongArg = true;
                }
                break;
                case 18: //"BulletFontName",
                {
                    assert( !pDocShell );
                    OUString sBulletFontName;
                    pData->aVal >>= sBulletFontName;
                    SwDocShell* pLclDocShell = pDocShell ? pDocShell : pDoc ? pDoc->GetDocShell() : 0;
                    if( !sBulletFontName.isEmpty() && pLclDocShell )
                    {
                        const SvxFontListItem* pFontListItem =
                                (const SvxFontListItem* )pLclDocShell
                                                    ->GetItem( SID_ATTR_CHAR_FONTLIST );
                        const FontList*  pList = pFontListItem->GetFontList();
                        vcl::FontInfo aInfo = pList->Get(
                            sBulletFontName, WEIGHT_NORMAL, ITALIC_NONE);
                        Font aFont(aInfo);
                        aFmt.SetBulletFont(&aFont);
                    }
                    else
                        m_sNewBulletFontNames[nIndex] = sBulletFontName;
                }
                break;
                case 19: //"BulletChar",
                {
                    assert( !pDocShell );
                    OUString aChar;
                    pData->aVal >>= aChar;
                    if(aChar.getLength() == 1)
                    {
                        aFmt.SetBulletChar(aChar.toChar());
                    }
                    else if(aChar.isEmpty())
                    {
                        // If w:lvlText's value is null - set bullet char to zero
                        aFmt.SetBulletChar(sal_Unicode(0x0));
                    }
                    else
                    {
                        bWrongArg = true;
                    }
                }
                break;
                case 20: //UNO_NAME_GRAPHIC_URL,
                {
                    assert( !pDocShell );
                    OUString sBrushURL;
                    pData->aVal >>= sBrushURL;
                    if(!pSetBrush)
                    {
                        const SvxBrushItem* pOrigBrush = aFmt.GetBrush();
                        if(pOrigBrush)
                        {
                            pSetBrush = new SvxBrushItem(*pOrigBrush);
                        }
                        else
                            pSetBrush = new SvxBrushItem(OUString(), OUString(), GPOS_AREA, RES_BACKGROUND);
                    }
                    pSetBrush->PutValue( pData->aVal, MID_GRAPHIC_URL );
                }
                break;
                case 21: //UNO_NAME_GRAPHIC_BITMAP,
                {
                    assert( !pDocShell );
                    uno::Reference< awt::XBitmap >* pBitmap = (uno::Reference< awt::XBitmap > *)pData->aVal.getValue();
                    if(pBitmap)
                    {
                        if(!pSetBrush)
                        {
                            const SvxBrushItem* pOrigBrush = aFmt.GetBrush();
                            if(pOrigBrush)
                            {
                                pSetBrush = new SvxBrushItem(*pOrigBrush);
                            }
                            else
                                pSetBrush = new SvxBrushItem(OUString(), OUString(), GPOS_AREA, RES_BACKGROUND);
                        }

                        BitmapEx aBmp = VCLUnoHelper::GetBitmap( *pBitmap );
                        Graphic aNewGr(aBmp);
                        pSetBrush->SetGraphic( aNewGr );
                    }
                    else
                        bWrongArg = true;
                }
                break;
                case 22: //UNO_NAME_GRAPHIC_SIZE,
                {
                    assert( !pDocShell );
                    if(!pSetSize)
                        pSetSize = new Size;
                    if(pData->aVal.getValueType() == ::cppu::UnoType<awt::Size>::get())
                    {
                         awt::Size* pSize =  (awt::Size*)pData->aVal.getValue();
                        pSize->Width = convertMm100ToTwip(pSize->Width);
                        pSize->Height = convertMm100ToTwip(pSize->Height);
                        pSetSize->Width() = pSize->Width;
                        pSetSize->Height() = pSize->Height;
                    }
                    else
                        bWrongArg = true;
                }
                break;
                case 23: //VertOrient
                {
                    assert( !pDocShell );
                    if(!pSetVOrient)
                    {
                        if(aFmt.GetGraphicOrientation())
                            pSetVOrient = (SwFmtVertOrient*)aFmt.GetGraphicOrientation()->Clone();
                        else
                            pSetVOrient = new SwFmtVertOrient;
                    }
                    ((SfxPoolItem*)pSetVOrient)->PutValue(pData->aVal, MID_VERTORIENT_ORIENT);
                }
                break;
                case 24: //"HeadingStyleName"
                {
                    assert( pDocShell );
                    OUString uTmp;
                    pData->aVal >>= uTmp;
                    OUString sStyleName;
                    SwStyleNameMapper::FillUIName(uTmp, sStyleName, nsSwGetPoolIdFromName::GET_POOLID_TXTCOLL, true );
                    const SwTxtFmtColls* pColls = pDocShell->GetDoc()->GetTxtFmtColls();
                    const size_t nCount = pColls->size();
                    for(size_t k = 0; k < nCount; ++k)
                    {
                        SwTxtFmtColl &rTxtColl = *((*pColls)[k]);
                        if(rTxtColl.IsDefault())
                            continue;
                        if ( rTxtColl.IsAssignedToListLevelOfOutlineStyle() &&
                             rTxtColl.GetAssignedOutlineStyleLevel() == nIndex &&
                             rTxtColl.GetName() != sStyleName )
                        {
                            rTxtColl.DeleteAssignmentToListLevelOfOutlineStyle();
                        }
                        else if ( rTxtColl.GetName() == sStyleName )
                        {
                            rTxtColl.AssignToListLevelOfOutlineStyle( nIndex );
                        }
                    }
                }
                break;
                case 25: // BulletRelSize - unsupported - only available in Impress
                break;
                case 26: // BulletColor - ignored too
                break;
            }
        }
        if(!bExcept && !bWrongArg && (pSetBrush || pSetSize || pSetVOrient))
        {
            if(!pSetBrush && aFmt.GetBrush())
                pSetBrush = new SvxBrushItem(*aFmt.GetBrush());

            if(pSetBrush)
            {
                if(!pSetVOrient && aFmt.GetGraphicOrientation())
                    pSetVOrient = new SwFmtVertOrient(*aFmt.GetGraphicOrientation());

                if(!pSetSize)
                {
                    pSetSize = new Size(aFmt.GetGraphicSize());
                    if(!pSetSize->Width() || !pSetSize->Height())
                    {
                        const Graphic* pGraphic = pSetBrush->GetGraphic();
                        if(pGraphic)
                            *pSetSize = ::GetGraphicSizeTwip(*pGraphic, 0);
                    }
                }
                sal_Int16 eOrient = pSetVOrient ?
                    (sal_Int16)pSetVOrient->GetVertOrient() : text::VertOrientation::NONE;
                aFmt.SetGraphicBrush( pSetBrush, pSetSize, text::VertOrientation::NONE == eOrient ? 0 : &eOrient );
            }
        }
        if((!bCharStyleNameSet || m_sNewCharStyleNames[nIndex].isEmpty()) &&
                aFmt.GetNumberingType() == NumberingType::BITMAP && !aFmt.GetCharFmt()
                    && !SwXNumberingRules::isInvalidStyle(m_sNewCharStyleNames[nIndex]))
        {
            OUString tmp;
            SwStyleNameMapper::FillProgName(RES_POOLCHR_BUL_LEVEL, tmp);
            m_sNewCharStyleNames[nIndex] = tmp;
        }
        delete pSetBrush;
        delete pSetSize;
        delete pSetVOrient;
      }
    for (PropValDataArr::const_iterator it = aPropertyValues.begin(); it != aPropertyValues.end(); ++it)
      delete *it;
    aPropertyValues.clear();

    if(bWrongArg)
        throw lang::IllegalArgumentException();
    else if(bExcept)
        throw uno::RuntimeException();
    rNumRule.Set( (sal_uInt16)nIndex, aFmt );
}

uno::Reference< XPropertySetInfo > SwXNumberingRules::getPropertySetInfo()
    throw(RuntimeException, std::exception)
{
    static uno::Reference< beans::XPropertySetInfo >  aRef = m_pPropertySet->getPropertySetInfo();
    return aRef;
}

void SwXNumberingRules::setPropertyValue( const OUString& rPropertyName, const Any& rValue )
    throw(UnknownPropertyException, PropertyVetoException,
        IllegalArgumentException, WrappedTargetException, RuntimeException, std::exception)
{
    SwNumRule* pDocRule = 0;
    SwNumRule* pCreatedRule = 0;
    if(!pNumRule)
    {
        if(!pNumRule && pDocShell)
        {
            pDocRule = new SwNumRule(*pDocShell->GetDoc()->GetOutlineNumRule());
        }
        else if(pDoc && !m_sCreatedNumRuleName.isEmpty())
        {
            pCreatedRule = pDoc->FindNumRulePtr(m_sCreatedNumRuleName);
        }

    }
    if(!pNumRule && !pDocRule && !pCreatedRule)
        throw RuntimeException();

    if(rPropertyName == UNO_NAME_IS_AUTOMATIC)
    {
        bool bVal = *(sal_Bool*)rValue.getValue();
        if(!pCreatedRule)
            pDocRule ? pDocRule->SetAutoRule(bVal) : pNumRule->SetAutoRule(bVal);
    }
    else if(rPropertyName == UNO_NAME_IS_CONTINUOUS_NUMBERING)
    {
        bool bVal = *(sal_Bool*)rValue.getValue();
        pDocRule ? pDocRule->SetContinusNum(bVal) :
            pCreatedRule ? pCreatedRule->SetContinusNum(bVal) : pNumRule->SetContinusNum(bVal);
    }
    else if(rPropertyName == UNO_NAME_NAME)
    {
        delete pDocRule;
        throw IllegalArgumentException();
    }
    else if(rPropertyName == UNO_NAME_IS_ABSOLUTE_MARGINS)
    {
        bool bVal = *(sal_Bool*)rValue.getValue();
        pDocRule ? pDocRule->SetAbsSpaces(bVal) :
            pCreatedRule ? pCreatedRule->SetAbsSpaces(bVal) : pNumRule->SetAbsSpaces(bVal);
    }
    else if(rPropertyName == UNO_NAME_NUMBERING_IS_OUTLINE)
    {
        bool bVal = *(sal_Bool*)rValue.getValue();
        SwNumRuleType eNumRuleType = bVal ? OUTLINE_RULE : NUM_RULE;
        pDocRule ? pDocRule->SetRuleType(eNumRuleType) :
            pCreatedRule ? pCreatedRule->SetRuleType(eNumRuleType) : pNumRule->SetRuleType(eNumRuleType);
    }
    else if(rPropertyName == UNO_NAME_DEFAULT_LIST_ID)
    {
        delete pDocRule;
        throw IllegalArgumentException();
    }
    else
        throw UnknownPropertyException();

    if(pDocRule)
    {
        pDocShell->GetDoc()->SetOutlineNumRule(*pDocRule);
        delete pDocRule;
    }
    else if(pCreatedRule)
    {
        pCreatedRule->Validate();
    }
}

Any SwXNumberingRules::getPropertyValue( const OUString& rPropertyName )
    throw(UnknownPropertyException, WrappedTargetException, RuntimeException, std::exception)
{
    Any aRet;
    const SwNumRule* pRule = pNumRule;
    if(!pRule && pDocShell)
        pRule = pDocShell->GetDoc()->GetOutlineNumRule();
    else if(pDoc && !m_sCreatedNumRuleName.isEmpty())
        pRule = pDoc->FindNumRulePtr( m_sCreatedNumRuleName );
    if(!pRule)
        throw RuntimeException();

    if(rPropertyName == UNO_NAME_IS_AUTOMATIC)
    {
        sal_Bool bVal = pRule->IsAutoRule();
        aRet.setValue(&bVal, ::getBooleanCppuType());
    }
    else if(rPropertyName == UNO_NAME_IS_CONTINUOUS_NUMBERING)
    {
        sal_Bool bVal = pRule->IsContinusNum();
        aRet.setValue(&bVal, ::getBooleanCppuType());
    }
    else if(rPropertyName == UNO_NAME_NAME)
        aRet <<= pRule->GetName();
    else if(rPropertyName == UNO_NAME_IS_ABSOLUTE_MARGINS)
    {
        sal_Bool bVal = pRule->IsAbsSpaces();
        aRet.setValue(&bVal, ::getBooleanCppuType());
    }
    else if(rPropertyName == UNO_NAME_NUMBERING_IS_OUTLINE)
    {
        sal_Bool bVal = pRule->IsOutlineRule();
        aRet.setValue(&bVal, ::getBooleanCppuType());
    }
    else if(rPropertyName == UNO_NAME_DEFAULT_LIST_ID)
    {
        OSL_ENSURE( !pRule->GetDefaultListId().isEmpty(),
                "<SwXNumberingRules::getPropertyValue(..)> - no default list id found. Serious defect -> please inform OD." );
        aRet <<= pRule->GetDefaultListId();
    }
    else
        throw UnknownPropertyException();
    return aRet;
}

void SwXNumberingRules::addPropertyChangeListener(
    const OUString& /*rPropertyName*/, const uno::Reference< XPropertyChangeListener >& /*xListener*/ )
        throw(UnknownPropertyException, WrappedTargetException, RuntimeException, std::exception)
{
}

void SwXNumberingRules::removePropertyChangeListener(
    const OUString& /*rPropertyName*/, const uno::Reference< XPropertyChangeListener >& /*xListener*/ )
        throw(UnknownPropertyException, WrappedTargetException, RuntimeException, std::exception)
{
}

void SwXNumberingRules::addVetoableChangeListener(
    const OUString& /*rPropertyName*/, const uno::Reference< XVetoableChangeListener >& /*xListener*/ )
        throw(UnknownPropertyException, WrappedTargetException, RuntimeException, std::exception)
{
}

void SwXNumberingRules::removeVetoableChangeListener(
    const OUString& /*rPropertyName*/, const uno::Reference< XVetoableChangeListener >& /*xListener*/ )
        throw(UnknownPropertyException, WrappedTargetException, RuntimeException, std::exception)
{
}

OUString SwXNumberingRules::getName() throw( RuntimeException, std::exception )
{
    if(pNumRule)
    {
        OUString aString;
        SwStyleNameMapper::FillProgName(pNumRule->GetName(), aString, nsSwGetPoolIdFromName::GET_POOLID_NUMRULE, true );
        return aString;
    }
    // consider chapter numbering <SwXNumberingRules>
    if ( pDocShell )
    {
        OUString aString;
        SwStyleNameMapper::FillProgName( pDocShell->GetDoc()->GetOutlineNumRule()->GetName(),
                                         aString, nsSwGetPoolIdFromName::GET_POOLID_NUMRULE, true );
        return aString;
    }
    return m_sCreatedNumRuleName;
}

void SwXNumberingRules::setName(const OUString& /*rName*/) throw( RuntimeException, std::exception )
{
    RuntimeException aExcept;
    aExcept.Message = "readonly";
    throw aExcept;
}

void SwXNumberingRules::Modify( const SfxPoolItem* pOld, const SfxPoolItem *pNew)
{
    ClientModify(this, pOld, pNew);
    if(!GetRegisteredIn())
    {
        if(bOwnNumRuleCreated)
            delete pNumRule;
        pNumRule = 0;
        pDoc = 0;
    }
}

OUString SwXChapterNumbering::getImplementationName(void) throw( RuntimeException, std::exception )
{
    return OUString("SwXChapterNumbering");
}

sal_Bool SwXChapterNumbering::supportsService(const OUString& rServiceName) throw( RuntimeException, std::exception )
{
    return cppu::supportsService(this, rServiceName);
}

Sequence< OUString > SwXChapterNumbering::getSupportedServiceNames(void) throw( RuntimeException, std::exception )
{
    Sequence< OUString > aRet(2);
    OUString* pArray = aRet.getArray();
    pArray[0] = "com.sun.star.text.ChapterNumbering";
    pArray[1] = "com.sun.star.text.NumberingRules";
    return aRet;
}

SwXChapterNumbering::SwXChapterNumbering(SwDocShell& rDocSh) :
    SwXNumberingRules(rDocSh)
{
}

SwXChapterNumbering::~SwXChapterNumbering()
{
}

OUString SwXTextColumns::getImplementationName(void) throw( RuntimeException, std::exception )
{
    return OUString("SwXTextColumns");
}

sal_Bool SwXTextColumns::supportsService(const OUString& rServiceName) throw( RuntimeException, std::exception )
{
    return cppu::supportsService(this, rServiceName);
}

Sequence< OUString > SwXTextColumns::getSupportedServiceNames(void) throw( RuntimeException, std::exception )
{
    Sequence< OUString > aRet(1);
    OUString* pArray = aRet.getArray();
    pArray[0] = "com.sun.star.text.TextColumns";
    return aRet;
}

SwXTextColumns::SwXTextColumns(sal_uInt16 nColCount) :
    nReference(0),
    bIsAutomaticWidth(true),
    nAutoDistance(0),
    m_pPropSet(aSwMapProvider.GetPropertySet(PROPERTY_MAP_TEXT_COLUMS)),
    nSepLineWidth(0),
    nSepLineColor(0), //black
    nSepLineHeightRelative(100),//full height
    nSepLineVertAlign(style::VerticalAlignment_MIDDLE),
    bSepLineIsOn(false),
    nSepLineStyle(API_COL_LINE_NONE) // None
{
    if(nColCount)
        setColumnCount(nColCount);
}

SwXTextColumns::SwXTextColumns(const SwFmtCol& rFmtCol) :
    nReference(0),
    aTextColumns(rFmtCol.GetNumCols()),
    bIsAutomaticWidth(rFmtCol.IsOrtho()),
    m_pPropSet(aSwMapProvider.GetPropertySet(PROPERTY_MAP_TEXT_COLUMS))
{
    const sal_uInt16 nItemGutterWidth = rFmtCol.GetGutterWidth();
    nAutoDistance = bIsAutomaticWidth ?
                        USHRT_MAX == nItemGutterWidth ? DEF_GUTTER_WIDTH : (sal_Int32)nItemGutterWidth
                        : 0;
    nAutoDistance = convertTwipToMm100(nAutoDistance);

    TextColumn* pColumns = aTextColumns.getArray();
    const SwColumns& rCols = rFmtCol.GetColumns();
    for(sal_Int32 i = 0; i < aTextColumns.getLength(); ++i)
    {
        const SwColumn* pCol = &rCols[i];

        pColumns[i].Width = pCol->GetWishWidth();
        nReference += pColumns[i].Width;
        pColumns[i].LeftMargin =    convertTwipToMm100(pCol->GetLeft ());
        pColumns[i].RightMargin =   convertTwipToMm100(pCol->GetRight());
    }
    if(!aTextColumns.getLength())
        nReference = USHRT_MAX;

    nSepLineWidth = rFmtCol.GetLineWidth();
    nSepLineColor = rFmtCol.GetLineColor().GetColor();
    nSepLineHeightRelative = rFmtCol.GetLineHeight();
    bSepLineIsOn = rFmtCol.GetLineAdj() != COLADJ_NONE;
    sal_Int8 nStyle = API_COL_LINE_NONE;
    switch (rFmtCol.GetLineStyle())
    {
        case table::BorderLineStyle::SOLID: nStyle = API_COL_LINE_SOLID; break;
        case table::BorderLineStyle::DOTTED: nStyle= API_COL_LINE_DOTTED; break;
        case table::BorderLineStyle::DASHED: nStyle= API_COL_LINE_DASHED; break;
        default: break;
    }
    nSepLineStyle = nStyle;
    switch(rFmtCol.GetLineAdj())
    {
        case COLADJ_TOP:    nSepLineVertAlign = style::VerticalAlignment_TOP;   break;
        case COLADJ_BOTTOM: nSepLineVertAlign = style::VerticalAlignment_BOTTOM;    break;
        case COLADJ_CENTER:
        case COLADJ_NONE:   nSepLineVertAlign = style::VerticalAlignment_MIDDLE;
    }
}

SwXTextColumns::~SwXTextColumns()
{
}

sal_Int32 SwXTextColumns::getReferenceValue(void) throw( uno::RuntimeException, std::exception )
{
    SolarMutexGuard aGuard;
    return nReference;
}

sal_Int16 SwXTextColumns::getColumnCount(void) throw( uno::RuntimeException, std::exception )
{
    SolarMutexGuard aGuard;
    return static_cast< sal_Int16>( aTextColumns.getLength() );
}

void SwXTextColumns::setColumnCount(sal_Int16 nColumns) throw( uno::RuntimeException, std::exception )
{
    SolarMutexGuard aGuard;
    if(nColumns <= 0)
        throw uno::RuntimeException();
    bIsAutomaticWidth = true;
    aTextColumns.realloc(nColumns);
     TextColumn* pCols = aTextColumns.getArray();
    nReference = USHRT_MAX;
    sal_Int32 nWidth = nReference / nColumns;
    sal_Int32 nDiff = nReference - nWidth * nColumns;
    sal_Int32 nDist = nAutoDistance / 2;
    for(sal_Int16 i = 0; i < nColumns; i++)
    {
        pCols[i].Width = nWidth;
        pCols[i].LeftMargin = i == 0 ? 0 : nDist;
        pCols[i].RightMargin = i == nColumns - 1 ? 0 : nDist;
    }
    pCols[nColumns - 1].Width += nDiff;
}

uno::Sequence< TextColumn > SwXTextColumns::getColumns(void) throw( uno::RuntimeException, std::exception )
{
    SolarMutexGuard aGuard;
    return aTextColumns;
}

void SwXTextColumns::setColumns(const uno::Sequence< TextColumn >& rColumns)
            throw( uno::RuntimeException, std::exception )
{
    SolarMutexGuard aGuard;
    sal_Int32 nReferenceTemp = 0;
    const TextColumn* prCols = rColumns.getConstArray();
    for(long i = 0; i < rColumns.getLength(); i++)
    {
        nReferenceTemp += prCols[i].Width;
    }
    bIsAutomaticWidth = false;
    nReference = !nReferenceTemp ? USHRT_MAX : nReferenceTemp;
    aTextColumns = rColumns;
}

uno::Reference< XPropertySetInfo > SwXTextColumns::getPropertySetInfo(  ) throw(RuntimeException, std::exception)
{
    static uno::Reference< beans::XPropertySetInfo >  aRef = m_pPropSet->getPropertySetInfo();
    return aRef;
}

void SwXTextColumns::setPropertyValue( const OUString& rPropertyName, const Any& aValue )
        throw(UnknownPropertyException, PropertyVetoException, IllegalArgumentException,
            WrappedTargetException, RuntimeException, std::exception)
{
    const SfxItemPropertySimpleEntry*  pEntry = m_pPropSet->getPropertyMap().getByName( rPropertyName );
    if (!pEntry)
        throw UnknownPropertyException("Unknown property: " + rPropertyName, static_cast < cppu::OWeakObject * > ( this ) );
    if ( pEntry->nFlags & PropertyAttribute::READONLY)
        throw PropertyVetoException("Property is read-only: " + rPropertyName, static_cast < cppu::OWeakObject * > ( this ) );

    switch(pEntry->nWID)
    {
        case WID_TXTCOL_LINE_WIDTH:
        {
            sal_Int32 nTmp = 0;
            aValue >>= nTmp;
            if(nTmp < 0)
                throw IllegalArgumentException();
            nSepLineWidth = convertMm100ToTwip(nTmp);
        }
        break;
        case WID_TXTCOL_LINE_COLOR:
            aValue >>= nSepLineColor;
        break;
        case WID_TXTCOL_LINE_STYLE:
        {
            aValue >>= nSepLineStyle;
        }
        break;
        case WID_TXTCOL_LINE_REL_HGT:
        {
            sal_Int8 nTmp = 0;
            aValue >>= nTmp;
            if(nTmp < 0)
                throw IllegalArgumentException();
            nSepLineHeightRelative = nTmp;
        }
        break;
        case WID_TXTCOL_LINE_ALIGN:
        {
            style::VerticalAlignment eAlign;
            if(!(aValue >>= eAlign) )
            {
                sal_Int8 nTmp = 0;
                if (! ( aValue >>= nTmp ) )
                    throw IllegalArgumentException();
                else
                    nSepLineVertAlign = nTmp;
            }
            else
                nSepLineVertAlign = static_cast< sal_Int8 >(eAlign);
        }
        break;
        case WID_TXTCOL_LINE_IS_ON:
            bSepLineIsOn = *(sal_Bool*)aValue.getValue();
        break;
        case WID_TXTCOL_AUTO_DISTANCE:
        {
            sal_Int32 nTmp = 0;
            aValue >>= nTmp;
            if(nTmp < 0 || nTmp >= nReference)
                throw IllegalArgumentException();
            nAutoDistance = nTmp;
            sal_Int32 nColumns = aTextColumns.getLength();
            TextColumn* pCols = aTextColumns.getArray();
            sal_Int32 nDist = nAutoDistance / 2;
            for(sal_Int32 i = 0; i < nColumns; i++)
            {
                pCols[i].LeftMargin = i == 0 ? 0 : nDist;
                pCols[i].RightMargin = i == nColumns - 1 ? 0 : nDist;
            }
        }
        break;
    }
}

Any SwXTextColumns::getPropertyValue( const OUString& rPropertyName )
        throw(UnknownPropertyException, WrappedTargetException, RuntimeException, std::exception)
{
    const SfxItemPropertySimpleEntry*  pEntry = m_pPropSet->getPropertyMap().getByName( rPropertyName );
    if (!pEntry)
        throw UnknownPropertyException("Unknown property: " + rPropertyName, static_cast < cppu::OWeakObject * > ( this ) );

    Any aRet;
    switch(pEntry->nWID)
    {
        case WID_TXTCOL_LINE_WIDTH:
            aRet <<= static_cast < sal_Int32 >(convertTwipToMm100(nSepLineWidth));
        break;
        case WID_TXTCOL_LINE_COLOR:
            aRet <<= nSepLineColor;
        break;
        case WID_TXTCOL_LINE_STYLE:
            aRet <<= nSepLineStyle;
        break;
        case WID_TXTCOL_LINE_REL_HGT:
            aRet <<= nSepLineHeightRelative;
        break;
        case WID_TXTCOL_LINE_ALIGN:
            aRet <<= (style::VerticalAlignment)nSepLineVertAlign;
        break;
        case WID_TXTCOL_LINE_IS_ON:
            aRet.setValue(&bSepLineIsOn, ::getBooleanCppuType());
        break;
        case WID_TXTCOL_IS_AUTOMATIC :
            aRet.setValue(&bIsAutomaticWidth, ::getBooleanCppuType());
        break;
        case WID_TXTCOL_AUTO_DISTANCE:
            aRet <<= nAutoDistance;
        break;
    }
    return aRet;
}

void SwXTextColumns::addPropertyChangeListener(
    const OUString& /*rPropertyName*/, const uno::Reference< XPropertyChangeListener >& /*xListener*/ )
        throw(UnknownPropertyException, WrappedTargetException, RuntimeException, std::exception)
{
}

void SwXTextColumns::removePropertyChangeListener(
    const OUString& /*rPropertyName*/, const uno::Reference< XPropertyChangeListener >& /*xListener*/ )
        throw(UnknownPropertyException, WrappedTargetException, RuntimeException, std::exception)
{
}

void SwXTextColumns::addVetoableChangeListener(
    const OUString& /*rPropertyName*/, const uno::Reference< XVetoableChangeListener >& /*xListener*/ )
        throw(UnknownPropertyException, WrappedTargetException, RuntimeException, std::exception)
{
}

void SwXTextColumns::removeVetoableChangeListener(
    const OUString& /*rPropertyName*/, const uno::Reference< XVetoableChangeListener >& /*xListener*/ )
        throw(UnknownPropertyException, WrappedTargetException, RuntimeException, std::exception)
{
}

namespace
{
    class theSwXTextColumnsUnoTunnelId : public rtl::Static< UnoTunnelIdInit, theSwXTextColumnsUnoTunnelId > {};
}

const uno::Sequence< sal_Int8 > & SwXTextColumns::getUnoTunnelId()
{
    return theSwXTextColumnsUnoTunnelId::get().getSeq();
}

sal_Int64 SAL_CALL SwXTextColumns::getSomething( const uno::Sequence< sal_Int8 >& rId )
    throw(uno::RuntimeException, std::exception)
{
    if( rId.getLength() == 16
        && 0 == memcmp( getUnoTunnelId().getConstArray(),
                                        rId.getConstArray(), 16 ) )
    {
        return sal::static_int_cast< sal_Int64 >( reinterpret_cast< sal_IntPtr >(this) );
    }
    return 0;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
