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

#include <com/sun/star/util/DateTime.hpp>
#include <com/sun/star/text/XTextTable.hpp>

#include <osl/mutex.hxx>
#include <vcl/svapp.hxx>
#include <comphelper/servicehelper.hxx>

#include <pagedesc.hxx>
#include "poolfmt.hxx"
#include <redline.hxx>
#include <section.hxx>
#include <unoprnms.hxx>
#include <unomid.h>
#include <unotextrange.hxx>
#include <unotextcursor.hxx>
#include <unoparagraph.hxx>
#include <unocoll.hxx>
#include <unomap.hxx>
#include <unocrsr.hxx>
#include <unoport.hxx>
#include <unoredline.hxx>
#include <doc.hxx>
#include <docary.hxx>

using namespace ::com::sun::star;

SwXRedlineText::SwXRedlineText(SwDoc* _pDoc, SwNodeIndex aIndex) :
    SwXText(_pDoc, CURSOR_REDLINE),
    aNodeIndex(aIndex)
{
}

const SwStartNode* SwXRedlineText::GetStartNode() const
{
    return aNodeIndex.GetNode().GetStartNode();
}

uno::Any SwXRedlineText::queryInterface( const uno::Type& rType )
    throw(uno::RuntimeException, std::exception)
{
    uno::Any aRet;

    if (cppu::UnoType<container::XEnumerationAccess>::get()== rType)
    {
        uno::Reference<container::XEnumerationAccess> aAccess = this;
        aRet <<= aAccess;
    }
    else
    {
        // delegate to SwXText and OWeakObject
        aRet = SwXText::queryInterface(rType);
        if(!aRet.hasValue())
        {
            aRet = OWeakObject::queryInterface(rType);
        }
    }

    return aRet;
}

uno::Sequence<uno::Type> SwXRedlineText::getTypes()
    throw(uno::RuntimeException, std::exception)
{
    // SwXText::getTypes()
    uno::Sequence<uno::Type> aTypes = SwXText::getTypes();

    // add container::XEnumerationAccess
    sal_Int32 nLength = aTypes.getLength();
    aTypes.realloc(nLength + 1);
    aTypes[nLength] = cppu::UnoType<container::XEnumerationAccess>::get();

    return aTypes;
}

uno::Sequence<sal_Int8> SwXRedlineText::getImplementationId()
    throw(uno::RuntimeException, std::exception)
{
    return css::uno::Sequence<sal_Int8>();
}

uno::Reference<text::XTextCursor> SwXRedlineText::createTextCursor(void)
    throw( uno::RuntimeException, std::exception )
{
    SolarMutexGuard aGuard;

    SwPosition aPos(aNodeIndex);
    SwXTextCursor *const pXCursor =
        new SwXTextCursor(*GetDoc(), this, CURSOR_REDLINE, aPos);
    SwUnoCrsr *const pUnoCursor = pXCursor->GetCursor();
    pUnoCursor->Move(fnMoveForward, fnGoNode);

    // #101929# prevent a newly created text cursor from running inside a table
    // because table cells have their own XText.
    // Patterned after SwXTextFrame::createTextCursor(void).

    // skip all tables at the beginning
    SwTableNode* pTableNode = pUnoCursor->GetNode().FindTableNode();
    SwCntntNode* pContentNode = NULL;
    bool bTable = pTableNode != NULL;
    while( pTableNode != NULL )
    {
        pUnoCursor->GetPoint()->nNode = *(pTableNode->EndOfSectionNode());
        pContentNode = GetDoc()->GetNodes().GoNext(&pUnoCursor->GetPoint()->nNode);
        pTableNode = pContentNode->FindTableNode();
    }
    if( pContentNode != NULL )
        pUnoCursor->GetPoint()->nContent.Assign( pContentNode, 0 );
    if( bTable && pUnoCursor->GetNode().FindSttNodeByType( SwNormalStartNode )
                                                            != GetStartNode() )
    {
        // We have gone too far and have left our own redline. This means that
        // no content node outside of a table could be found, and therefore we
        // except.
        uno::RuntimeException aExcept;
        aExcept.Message =
            "No content node found that is inside this change section "
            "but outside of a table";
        throw aExcept;
    }

    return static_cast<text::XWordCursor*>(pXCursor);
}

uno::Reference<text::XTextCursor> SwXRedlineText::createTextCursorByRange(
    const uno::Reference<text::XTextRange> & aTextRange)
        throw( uno::RuntimeException, std::exception )
{
    uno::Reference<text::XTextCursor> xCursor = createTextCursor();
    xCursor->gotoRange(aTextRange->getStart(), sal_False);
    xCursor->gotoRange(aTextRange->getEnd(), sal_True);
    return xCursor;
}

uno::Reference<container::XEnumeration> SwXRedlineText::createEnumeration(void)
    throw( uno::RuntimeException, std::exception )
{
    SolarMutexGuard aGuard;
    SwPaM aPam(aNodeIndex);
    aPam.Move(fnMoveForward, fnGoNode);
    SAL_WNODEPRECATED_DECLARATIONS_PUSH
    ::std::auto_ptr<SwUnoCrsr> pUnoCursor(
        GetDoc()->CreateUnoCrsr(*aPam.Start(), false));
    SAL_WNODEPRECATED_DECLARATIONS_POP
    return new SwXParagraphEnumeration(this, pUnoCursor, CURSOR_REDLINE);
}

uno::Type SwXRedlineText::getElementType(  ) throw(uno::RuntimeException, std::exception)
{
    return cppu::UnoType<text::XTextRange>::get();
}

sal_Bool SwXRedlineText::hasElements(  ) throw(uno::RuntimeException, std::exception)
{
    return sal_True;    // we always have a content index
}

SwXRedlinePortion::SwXRedlinePortion(SwRangeRedline const& rRedline,
        SwUnoCrsr const*const pPortionCrsr,
        uno::Reference< text::XText > const& xParent, bool const bStart)
    : SwXTextPortion(pPortionCrsr, xParent,
            (bStart) ? PORTION_REDLINE_START : PORTION_REDLINE_END)
    , m_rRedline(rRedline)
{
    SetCollapsed(!m_rRedline.HasMark());
}

SwXRedlinePortion::~SwXRedlinePortion()
{
}

static util::DateTime lcl_DateTimeToUno(const DateTime& rDT)
{
    util::DateTime aRetDT;
    aRetDT.Year = rDT.GetYear();
    aRetDT.Month= rDT.GetMonth();
    aRetDT.Day      = rDT.GetDay();
    aRetDT.Hours    = rDT.GetHour();
    aRetDT.Minutes = rDT.GetMin();
    aRetDT.Seconds = rDT.GetSec();
    aRetDT.NanoSeconds = rDT.GetNanoSec();
    return aRetDT;
}

static OUString lcl_RedlineTypeToOUString(RedlineType_t eType)
{
    OUString sRet;
    switch(eType & nsRedlineType_t::REDLINE_NO_FLAG_MASK)
    {
        case nsRedlineType_t::REDLINE_INSERT: sRet = "Insert"; break;
        case nsRedlineType_t::REDLINE_DELETE: sRet = "Delete"; break;
        case nsRedlineType_t::REDLINE_FORMAT: sRet = "Format"; break;
        case nsRedlineType_t::REDLINE_TABLE:  sRet = "TextTable"; break;
        case nsRedlineType_t::REDLINE_FMTCOLL:sRet = "Style"; break;
    }
    return sRet;
}

static uno::Sequence<beans::PropertyValue> lcl_GetSuccessorProperties(const SwRangeRedline& rRedline)
{
    uno::Sequence<beans::PropertyValue> aValues(4);

    const SwRedlineData* pNext = rRedline.GetRedlineData().Next();
    if(pNext)
    {
        beans::PropertyValue* pValues = aValues.getArray();
        pValues[0].Name = UNO_NAME_REDLINE_AUTHOR;
        // GetAuthorString(n) walks the SwRedlineData* chain;
        // here we always need element 1
        pValues[0].Value <<= rRedline.GetAuthorString(1);
        pValues[1].Name = UNO_NAME_REDLINE_DATE_TIME;
        pValues[1].Value <<= lcl_DateTimeToUno(pNext->GetTimeStamp());
        pValues[2].Name = UNO_NAME_REDLINE_COMMENT;
        pValues[2].Value <<= pNext->GetComment();
        pValues[3].Name = UNO_NAME_REDLINE_TYPE;
        pValues[3].Value <<= lcl_RedlineTypeToOUString(pNext->GetType());
    }
    return aValues;
}

uno::Any SwXRedlinePortion::getPropertyValue( const OUString& rPropertyName )
        throw(beans::UnknownPropertyException, lang::WrappedTargetException, uno::RuntimeException, std::exception)
{
    SolarMutexGuard aGuard;
    Validate();
    uno::Any aRet;
    if(rPropertyName == UNO_NAME_REDLINE_TEXT)
    {
        SwNodeIndex* pNodeIdx = m_rRedline.GetContentIdx();
        if(pNodeIdx )
        {
            if ( 1 < ( pNodeIdx->GetNode().EndOfSectionIndex() - pNodeIdx->GetNode().GetIndex() ) )
            {
                SwUnoCrsr* pUnoCrsr = GetCursor();
                uno::Reference<text::XText> xRet = new SwXRedlineText(pUnoCrsr->GetDoc(), *pNodeIdx);
                aRet <<= xRet;
            }
            else {
                OSL_FAIL("Empty section in redline portion! (end node immediately follows start node)");
            }
        }
    }
    else
    {
        aRet = GetPropertyValue(rPropertyName, m_rRedline);
        if(!aRet.hasValue() &&
           rPropertyName != UNO_NAME_REDLINE_SUCCESSOR_DATA)
            aRet = SwXTextPortion::getPropertyValue(rPropertyName);
    }
    return aRet;
}

void SwXRedlinePortion::Validate() throw( uno::RuntimeException )
{
    SwUnoCrsr* pUnoCrsr = GetCursor();
    if(!pUnoCrsr)
        throw uno::RuntimeException();
    //search for the redline
    SwDoc* pDoc = pUnoCrsr->GetDoc();
    const SwRedlineTbl& rRedTbl = pDoc->GetRedlineTbl();
    bool bFound = false;
    for(size_t nRed = 0; nRed < rRedTbl.size() && !bFound; nRed++)
        bFound = &m_rRedline == rRedTbl[nRed];
    if(!bFound)
        throw uno::RuntimeException();
}

uno::Sequence< sal_Int8 > SAL_CALL SwXRedlinePortion::getImplementationId(  ) throw(uno::RuntimeException, std::exception)
{
    return css::uno::Sequence<sal_Int8>();
}

uno::Any  SwXRedlinePortion::GetPropertyValue( const OUString& rPropertyName, const SwRangeRedline& rRedline ) throw()
{
    uno::Any aRet;
    if(rPropertyName == UNO_NAME_REDLINE_AUTHOR)
        aRet <<= rRedline.GetAuthorString();
    else if(rPropertyName == UNO_NAME_REDLINE_DATE_TIME)
    {
        aRet <<= lcl_DateTimeToUno(rRedline.GetTimeStamp());
    }
    else if(rPropertyName == UNO_NAME_REDLINE_COMMENT)
        aRet <<= rRedline.GetComment();
    else if(rPropertyName == UNO_NAME_REDLINE_TYPE)
    {
        aRet <<= lcl_RedlineTypeToOUString(rRedline.GetType());
    }
    else if(rPropertyName == UNO_NAME_REDLINE_SUCCESSOR_DATA)
    {
        if(rRedline.GetRedlineData().Next())
            aRet <<= lcl_GetSuccessorProperties(rRedline);
    }
    else if (rPropertyName == UNO_NAME_REDLINE_IDENTIFIER)
    {
        aRet <<= OUString::number(
            sal::static_int_cast< sal_Int64 >( reinterpret_cast< sal_IntPtr >(&rRedline) ) );
    }
    else if (rPropertyName == UNO_NAME_IS_IN_HEADER_FOOTER)
    {
        sal_Bool bRet =
            rRedline.GetDoc()->IsInHeaderFooter( rRedline.GetPoint()->nNode );
        aRet.setValue(&bRet, ::getBooleanCppuType());
    }
    else if (rPropertyName == UNO_NAME_MERGE_LAST_PARA)
    {
        sal_Bool bRet = !rRedline.IsDelLastPara();
        aRet.setValue( &bRet, ::getBooleanCppuType() );
    }
    return aRet;
}

uno::Sequence< beans::PropertyValue > SwXRedlinePortion::CreateRedlineProperties(
    const SwRangeRedline& rRedline, bool bIsStart ) throw()
{
    uno::Sequence< beans::PropertyValue > aRet(11);
    const SwRedlineData* pNext = rRedline.GetRedlineData().Next();
    beans::PropertyValue* pRet = aRet.getArray();

    sal_Int32 nPropIdx  = 0;
    pRet[nPropIdx].Name = UNO_NAME_REDLINE_AUTHOR;
    pRet[nPropIdx++].Value <<= rRedline.GetAuthorString();
    pRet[nPropIdx].Name = UNO_NAME_REDLINE_DATE_TIME;
    pRet[nPropIdx++].Value <<= lcl_DateTimeToUno(rRedline.GetTimeStamp());
    pRet[nPropIdx].Name = UNO_NAME_REDLINE_COMMENT;
    pRet[nPropIdx++].Value <<= rRedline.GetComment();
    pRet[nPropIdx].Name = UNO_NAME_REDLINE_TYPE;
    pRet[nPropIdx++].Value <<= lcl_RedlineTypeToOUString(rRedline.GetType());
    pRet[nPropIdx].Name = UNO_NAME_REDLINE_IDENTIFIER;
    pRet[nPropIdx++].Value <<= OUString::number(
        sal::static_int_cast< sal_Int64 >( reinterpret_cast< sal_IntPtr >(&rRedline) ) );
    pRet[nPropIdx].Name = UNO_NAME_IS_COLLAPSED;
    sal_Bool bTmp = !rRedline.HasMark();
    pRet[nPropIdx++].Value.setValue(&bTmp, ::getBooleanCppuType()) ;

    pRet[nPropIdx].Name = UNO_NAME_IS_START;
    pRet[nPropIdx++].Value.setValue(&bIsStart, ::getBooleanCppuType()) ;

    bTmp = !rRedline.IsDelLastPara();
    pRet[nPropIdx].Name = UNO_NAME_MERGE_LAST_PARA;
    pRet[nPropIdx++].Value.setValue(&bTmp, ::getBooleanCppuType()) ;

    SwNodeIndex* pNodeIdx = rRedline.GetContentIdx();
    if(pNodeIdx )
    {
        if ( 1 < ( pNodeIdx->GetNode().EndOfSectionIndex() - pNodeIdx->GetNode().GetIndex() ) )
        {
            uno::Reference<text::XText> xRet = new SwXRedlineText(rRedline.GetDoc(), *pNodeIdx);
            pRet[nPropIdx].Name = UNO_NAME_REDLINE_TEXT;
            pRet[nPropIdx++].Value <<= xRet;
        }
        else {
            OSL_FAIL("Empty section in redline portion! (end node immediately follows start node)");
        }
    }
    if(pNext)
    {
        pRet[nPropIdx].Name = UNO_NAME_REDLINE_SUCCESSOR_DATA;
        pRet[nPropIdx++].Value <<= lcl_GetSuccessorProperties(rRedline);
    }
    aRet.realloc(nPropIdx);
    return aRet;
}

TYPEINIT1(SwXRedline, SwClient);
SwXRedline::SwXRedline(SwRangeRedline& rRedline, SwDoc& rDoc) :
    SwXText(&rDoc, CURSOR_REDLINE),
    pDoc(&rDoc),
    pRedline(&rRedline)
{
    pDoc->GetPageDescFromPool(RES_POOLPAGE_STANDARD)->Add(this);
}

SwXRedline::~SwXRedline()
{
}

uno::Reference< beans::XPropertySetInfo > SwXRedline::getPropertySetInfo(  ) throw(uno::RuntimeException, std::exception)
{
    static uno::Reference< beans::XPropertySetInfo >  xRef =
        aSwMapProvider.GetPropertySet(PROPERTY_MAP_REDLINE)->getPropertySetInfo();
    return xRef;
}

void SwXRedline::setPropertyValue( const OUString& rPropertyName, const uno::Any& aValue )
    throw(beans::UnknownPropertyException, beans::PropertyVetoException, lang::IllegalArgumentException,
        lang::WrappedTargetException, uno::RuntimeException, std::exception)
{
    SolarMutexGuard aGuard;
    if(!pDoc)
        throw uno::RuntimeException();
    if(rPropertyName == UNO_NAME_REDLINE_AUTHOR)
    {
        OSL_FAIL("currently not available");
    }
    else if(rPropertyName == UNO_NAME_REDLINE_DATE_TIME)
    {
        OSL_FAIL("currently not available");
    }
    else if(rPropertyName == UNO_NAME_REDLINE_COMMENT)
    {
        OUString sTmp; aValue >>= sTmp;
        pRedline->SetComment(sTmp);
    }
    else if(rPropertyName == UNO_NAME_REDLINE_TYPE)
    {
        OSL_FAIL("currently not available");
        OUString sTmp; aValue >>= sTmp;
        if(sTmp.isEmpty())
            throw lang::IllegalArgumentException();
    }
    else if(rPropertyName == UNO_NAME_REDLINE_SUCCESSOR_DATA)
    {
        OSL_FAIL("currently not available");
    }
    else
    {
        throw lang::IllegalArgumentException();
    }
}

uno::Any SwXRedline::getPropertyValue( const OUString& rPropertyName )
    throw(beans::UnknownPropertyException, lang::WrappedTargetException, uno::RuntimeException, std::exception)
{
    SolarMutexGuard aGuard;
    if(!pDoc)
        throw uno::RuntimeException();
    uno::Any aRet;
    bool bStart = rPropertyName == UNO_NAME_REDLINE_START;
    if(bStart ||
        rPropertyName == UNO_NAME_REDLINE_END)
    {
        uno::Reference<XInterface> xRet;
        SwNode* pNode = &pRedline->GetNode();
        if(!bStart && pRedline->HasMark())
            pNode = &pRedline->GetNode(false);
        switch(pNode->GetNodeType())
        {
            case ND_SECTIONNODE:
            {
                SwSectionNode* pSectNode = pNode->GetSectionNode();
                OSL_ENSURE(pSectNode, "No section node!");
                xRet = SwXTextSections::GetObject( *pSectNode->GetSection().GetFmt() );
            }
            break;
            case ND_TABLENODE :
            {
                SwTableNode* pTblNode = pNode->GetTableNode();
                OSL_ENSURE(pTblNode, "No table node!");
                SwTable& rTbl = pTblNode->GetTable();
                SwFrmFmt* pTblFmt = rTbl.GetFrmFmt();
                xRet = SwXTextTables::GetObject( *pTblFmt );
            }
            break;
            case ND_TEXTNODE :
            {
                SwPosition* pPoint = 0;
                if(bStart || !pRedline->HasMark())
                    pPoint = pRedline->GetPoint();
                else
                    pPoint = pRedline->GetMark();
                const uno::Reference<text::XTextRange> xRange =
                    SwXTextRange::CreateXTextRange(*pDoc, *pPoint, 0);
                xRet = xRange.get();
            }
            break;
            default:
                OSL_FAIL("illegal node type");
        }
        aRet <<= xRet;
    }
    else if(rPropertyName == UNO_NAME_REDLINE_TEXT)
    {
        SwNodeIndex* pNodeIdx = pRedline->GetContentIdx();
        if( pNodeIdx )
        {
            if ( 1 < ( pNodeIdx->GetNode().EndOfSectionIndex() - pNodeIdx->GetNode().GetIndex() ) )
            {
                uno::Reference<text::XText> xRet = new SwXRedlineText(pDoc, *pNodeIdx);
                aRet <<= xRet;
            }
            else {
                OSL_FAIL("Empty section in redline portion! (end node immediately follows start node)");
            }
        }
    }
    else
        aRet = SwXRedlinePortion::GetPropertyValue(rPropertyName, *pRedline);
    return aRet;
}

void SwXRedline::addPropertyChangeListener(
    const OUString& /*aPropertyName*/,
    const uno::Reference< beans::XPropertyChangeListener >& /*xListener*/ )
        throw(beans::UnknownPropertyException, lang::WrappedTargetException, uno::RuntimeException, std::exception)
{
}

void SwXRedline::removePropertyChangeListener(
    const OUString& /*aPropertyName*/, const uno::Reference< beans::XPropertyChangeListener >& /*aListener*/ )
        throw(beans::UnknownPropertyException, lang::WrappedTargetException, uno::RuntimeException, std::exception)
{
}

void SwXRedline::addVetoableChangeListener(
    const OUString& /*PropertyName*/, const uno::Reference< beans::XVetoableChangeListener >& /*aListener*/ )
        throw(beans::UnknownPropertyException, lang::WrappedTargetException, uno::RuntimeException, std::exception)
{
}

void SwXRedline::removeVetoableChangeListener(
    const OUString& /*PropertyName*/, const uno::Reference< beans::XVetoableChangeListener >& /*aListener*/ )
        throw(beans::UnknownPropertyException, lang::WrappedTargetException, uno::RuntimeException, std::exception)
{
}

void SwXRedline::Modify( const SfxPoolItem* pOld, const SfxPoolItem *pNew)
{
    ClientModify(this, pOld, pNew);
    if(!GetRegisteredIn())
      {
        pDoc = 0;
        pRedline = 0;
    }
}

uno::Reference< container::XEnumeration >  SwXRedline::createEnumeration(void) throw( uno::RuntimeException, std::exception )
{
    SolarMutexGuard aGuard;
    uno::Reference< container::XEnumeration > xRet;
    if(!pDoc)
        throw uno::RuntimeException();

    SwNodeIndex* pNodeIndex = pRedline->GetContentIdx();
    if(pNodeIndex)
    {
        SwPaM aPam(*pNodeIndex);
        aPam.Move(fnMoveForward, fnGoNode);
        SAL_WNODEPRECATED_DECLARATIONS_PUSH
        ::std::auto_ptr<SwUnoCrsr> pUnoCursor(
            GetDoc()->CreateUnoCrsr(*aPam.Start(), false));
        SAL_WNODEPRECATED_DECLARATIONS_POP
        xRet = new SwXParagraphEnumeration(this, pUnoCursor, CURSOR_REDLINE);
    }
    return xRet;
}

uno::Type SwXRedline::getElementType(  ) throw(uno::RuntimeException, std::exception)
{
    return cppu::UnoType<text::XTextRange>::get();
}

sal_Bool SwXRedline::hasElements(  ) throw(uno::RuntimeException, std::exception)
{
    if(!pDoc)
        throw uno::RuntimeException();
    return 0 != pRedline->GetContentIdx();
}

uno::Reference< text::XTextCursor >  SwXRedline::createTextCursor(void) throw( uno::RuntimeException, std::exception )
{
    SolarMutexGuard aGuard;
    if(!pDoc)
        throw uno::RuntimeException();

    uno::Reference< text::XTextCursor >     xRet;
    SwNodeIndex* pNodeIndex = pRedline->GetContentIdx();
    if(pNodeIndex)
    {
        SwPosition aPos(*pNodeIndex);
        SwXTextCursor *const pXCursor =
            new SwXTextCursor(*pDoc, this, CURSOR_REDLINE, aPos);
        SwUnoCrsr *const pUnoCrsr = pXCursor->GetCursor();
        pUnoCrsr->Move(fnMoveForward, fnGoNode);

        // is here a table?
        SwTableNode* pTblNode = pUnoCrsr->GetNode().FindTableNode();
        SwCntntNode* pCont = 0;
        while( pTblNode )
        {
            pUnoCrsr->GetPoint()->nNode = *pTblNode->EndOfSectionNode();
            pCont = GetDoc()->GetNodes().GoNext(&pUnoCrsr->GetPoint()->nNode);
            pTblNode = pCont->FindTableNode();
        }
        if(pCont)
            pUnoCrsr->GetPoint()->nContent.Assign(pCont, 0);
        xRet = static_cast<text::XWordCursor*>(pXCursor);
    }
    else
    {
        throw uno::RuntimeException();
    }
    return xRet;
}

uno::Reference< text::XTextCursor >  SwXRedline::createTextCursorByRange(
    const uno::Reference< text::XTextRange > & /*aTextPosition*/)
        throw( uno::RuntimeException, std::exception )
{
    throw uno::RuntimeException();
}

uno::Any SwXRedline::queryInterface( const uno::Type& rType )
    throw(uno::RuntimeException, std::exception)
{
    uno::Any aRet = SwXText::queryInterface(rType);
    if(!aRet.hasValue())
    {
        aRet = SwXRedlineBaseClass::queryInterface(rType);
    }
    return aRet;
}

uno::Sequence<uno::Type> SwXRedline::getTypes()
    throw(uno::RuntimeException, std::exception)
{
    uno::Sequence<uno::Type> aTypes = SwXText::getTypes();
    uno::Sequence<uno::Type> aBaseTypes = SwXRedlineBaseClass::getTypes();
    const uno::Type* pBaseTypes = aBaseTypes.getConstArray();
    sal_Int32 nCurType = aTypes.getLength();
    aTypes.realloc(aTypes.getLength() + aBaseTypes.getLength());
    uno::Type* pTypes = aTypes.getArray();
    for(sal_Int32 nType = 0; nType < aBaseTypes.getLength(); nType++)
        pTypes[nCurType++] = pBaseTypes[nType];
    return aTypes;
}

uno::Sequence<sal_Int8> SwXRedline::getImplementationId()
    throw(uno::RuntimeException, std::exception)
{
    return css::uno::Sequence<sal_Int8>();
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
