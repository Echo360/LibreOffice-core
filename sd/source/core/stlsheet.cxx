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

#include <com/sun/star/beans/PropertyAttribute.hpp>
#include <com/sun/star/lang/DisposedException.hpp>
#include <com/sun/star/style/XStyle.hpp>

#include <osl/mutex.hxx>
#include <vcl/svapp.hxx>
#include <cppuhelper/supportsservice.hxx>
#include <boost/bind.hpp>

#include <editeng/outliner.hxx>
#include <editeng/eeitem.hxx>
#include <editeng/fhgtitem.hxx>
#include <svx/svdoattr.hxx>
#include <editeng/ulspitem.hxx>
#include <svl/smplhint.hxx>
#include <svl/itemset.hxx>

#include <svx/sdr/properties/attributeproperties.hxx>
#include <svx/xflbmtit.hxx>
#include <svx/xflbstit.hxx>
#include <editeng/bulletitem.hxx>
#include <editeng/lrspitem.hxx>
#include <svx/unoshprp.hxx>
#include <svx/unoshape.hxx>
#include <svx/svdpool.hxx>
#include "stlsheet.hxx"
#include "sdresid.hxx"
#include "sdpage.hxx"
#include "drawdoc.hxx"
#include "stlpool.hxx"
#include "glob.hrc"
#include "app.hrc"
#include "glob.hxx"
#include "helpids.h"
#include "../ui/inc/DrawViewShell.hxx"
#include "../ui/inc/ViewShellBase.hxx"
#include <editeng/boxitem.hxx>

#include <boost/make_shared.hpp>

using ::osl::MutexGuard;
using ::osl::ClearableMutexGuard;
using ::cppu::OInterfaceContainerHelper;
using ::com::sun::star::table::BorderLine;
using namespace ::com::sun::star::uno;
using namespace ::com::sun::star::util;
using namespace ::com::sun::star::lang;
using namespace ::com::sun::star::style;
using namespace ::com::sun::star::beans;
using namespace ::com::sun::star::container;
using namespace ::com::sun::star::drawing;

#define WID_STYLE_HIDDEN    7997
#define WID_STYLE_DISPNAME  7998
#define WID_STYLE_FAMILY    7999

static SvxItemPropertySet& GetStylePropertySet()
{
    static const SfxItemPropertyMapEntry aFullPropertyMap_Impl[] =
    {
        { OUString("Family"),                 WID_STYLE_FAMILY,       ::cppu::UnoType<OUString>::get(), PropertyAttribute::READONLY,    0},
        { OUString("UserDefinedAttributes"),  SDRATTR_XMLATTRIBUTES,  cppu::UnoType<XNameContainer>::get(), 0,     0},
        { OUString("DisplayName"),            WID_STYLE_DISPNAME,     ::cppu::UnoType<OUString>::get(), PropertyAttribute::READONLY,    0},
        { OUString("Hidden"),                 WID_STYLE_HIDDEN,       ::getCppuType((bool*)0),       0,     0},

        SVX_UNOEDIT_NUMBERING_PROPERTIE,
        SHADOW_PROPERTIES
        LINE_PROPERTIES
        LINE_PROPERTIES_START_END
        FILL_PROPERTIES
        EDGERADIUS_PROPERTIES
        TEXT_PROPERTIES_DEFAULTS
        CONNECTOR_PROPERTIES
        SPECIAL_DIMENSIONING_PROPERTIES_DEFAULTS
        { OUString("TopBorder"),                    SDRATTR_TABLE_BORDER,           ::cppu::UnoType<BorderLine>::get(), 0, TOP_BORDER }, \
        { OUString("BottomBorder"),                 SDRATTR_TABLE_BORDER,           ::cppu::UnoType<BorderLine>::get(), 0, BOTTOM_BORDER }, \
        { OUString("LeftBorder"),                   SDRATTR_TABLE_BORDER,           ::cppu::UnoType<BorderLine>::get(), 0, LEFT_BORDER }, \
        { OUString("RightBorder"),                  SDRATTR_TABLE_BORDER,           ::cppu::UnoType<BorderLine>::get(), 0, RIGHT_BORDER }, \
        { OUString(), 0, css::uno::Type(), 0, 0 }
    };

    static SvxItemPropertySet aPropSet( aFullPropertyMap_Impl, SdrObject::GetGlobalDrawObjectItemPool() );
    return aPropSet;
}

class ModifyListenerForewarder : public SfxListener
{
public:
    ModifyListenerForewarder( SdStyleSheet* pStyleSheet );

    virtual void Notify(SfxBroadcaster& rBC, const SfxHint& rHint) SAL_OVERRIDE;

private:
    SdStyleSheet* mpStyleSheet;
};

ModifyListenerForewarder::ModifyListenerForewarder( SdStyleSheet* pStyleSheet )
: mpStyleSheet( pStyleSheet )
{
    if( pStyleSheet )
    {
        SfxBroadcaster& rBC = static_cast< SfxBroadcaster& >( *pStyleSheet );
        StartListening( rBC );
    }
}

void ModifyListenerForewarder::Notify(SfxBroadcaster& /*rBC*/, const SfxHint& /*rHint*/)
{
    if( mpStyleSheet )
        mpStyleSheet->notifyModifyListener();
}

SdStyleSheet::SdStyleSheet(const OUString& rDisplayName, SfxStyleSheetBasePool& _rPool, SfxStyleFamily eFamily, sal_uInt16 _nMask)
: SdStyleSheetBase( OUString( rDisplayName ), _rPool, eFamily, _nMask)
, ::cppu::BaseMutex()
, msApiName( rDisplayName )
, mxPool( const_cast< SfxStyleSheetBasePool* >(&_rPool) )
, mrBHelper( m_aMutex )
{
}

SdStyleSheet::SdStyleSheet( const SdStyleSheet & r )
: SdStyleSheetBase( r )
, ::cppu::BaseMutex()
, msApiName( r.msApiName )
, mxPool( r.mxPool )
, mrBHelper( m_aMutex )
{
}

SdStyleSheet::~SdStyleSheet()
{
    delete pSet;
    pSet = NULL;    // that following destructors also get a change
}

void SdStyleSheet::SetApiName( const OUString& rApiName )
{
    msApiName = rApiName;
}

OUString SdStyleSheet::GetApiName() const
{
    if( !msApiName.isEmpty() )
        return msApiName;
    else
        return GetName();
}

void SdStyleSheet::Load (SvStream& rIn, sal_uInt16 nVersion)
{
    SfxStyleSheetBase::Load(rIn, nVersion);

    /* previously, the default mask was 0xAFFE. The needed flags were masked
       from this mask. Now the flag SFXSTYLEBIT_READONLY was introduced and with
       this, all style sheets are read only. Since no style sheet should be read
       only in Draw, we reset the flag here.  */
    nMask &= ~SFXSTYLEBIT_READONLY;
}

void SdStyleSheet::Store(SvStream& rOut)
{
    SfxStyleSheetBase::Store(rOut);
}

bool SdStyleSheet::SetParent(const OUString& rParentName)
{
    bool bResult = false;

    if (SfxStyleSheet::SetParent(rParentName))
    {
        // PseudoStyleSheets do not have their own ItemSets
        if (nFamily != SD_STYLE_FAMILY_PSEUDO)
        {
            if( !rParentName.isEmpty() )
            {
                SfxStyleSheetBase* pStyle = pPool->Find(rParentName, nFamily);
                if (pStyle)
                {
                    bResult = true;
                    SfxItemSet& rParentSet = pStyle->GetItemSet();
                    GetItemSet().SetParent(&rParentSet);
                    Broadcast( SfxSimpleHint( SFX_HINT_DATACHANGED ) );
                }
            }
            else
            {
                bResult = true;
                GetItemSet().SetParent(NULL);
                Broadcast( SfxSimpleHint( SFX_HINT_DATACHANGED ) );
            }
        }
        else
        {
            bResult = true;
        }
    }
    return bResult;
}

/**
 * create if necessary and return ItemSets
 */
SfxItemSet& SdStyleSheet::GetItemSet()
{
    if (nFamily == SD_STYLE_FAMILY_GRAPHICS || nFamily == SD_STYLE_FAMILY_MASTERPAGE)
    {
        // we create the ItemSet 'on demand' if necessary
        if (!pSet)
        {
            sal_uInt16 nWhichPairTable[] = { XATTR_LINE_FIRST,              XATTR_LINE_LAST,
                                         XATTR_FILL_FIRST,              XATTR_FILL_LAST,

                                        SDRATTR_SHADOW_FIRST,           SDRATTR_SHADOW_LAST,
                                        SDRATTR_TEXT_MINFRAMEHEIGHT,    SDRATTR_TEXT_CONTOURFRAME,

                                        SDRATTR_TEXT_WORDWRAP,          SDRATTR_TEXT_AUTOGROWSIZE,

                                        SDRATTR_EDGE_FIRST,             SDRATTR_EDGE_LAST,
                                        SDRATTR_MEASURE_FIRST,          SDRATTR_MEASURE_LAST,

                                        EE_PARA_START,                  EE_CHAR_END,

                                        SDRATTR_XMLATTRIBUTES,          SDRATTR_TEXT_USEFIXEDCELLHEIGHT,

                                        SDRATTR_3D_FIRST, SDRATTR_3D_LAST,
                                        0, 0 };

            pSet = new SfxItemSet(GetPool().GetPool(), nWhichPairTable);
        }

        return *pSet;
    }

    else if( nFamily == SD_STYLE_FAMILY_CELL )
    {
        if (!pSet)
        {
            sal_uInt16 nWhichPairTable[] = { XATTR_LINE_FIRST,              XATTR_LINE_LAST,
                                         XATTR_FILL_FIRST,              XATTR_FILL_LAST,

                                        SDRATTR_SHADOW_FIRST,           SDRATTR_SHADOW_LAST,
                                        SDRATTR_TEXT_MINFRAMEHEIGHT,    SDRATTR_TEXT_CONTOURFRAME,

                                        SDRATTR_TEXT_WORDWRAP,          SDRATTR_TEXT_AUTOGROWSIZE,

                                        EE_PARA_START,                  EE_CHAR_END,

                                        SDRATTR_TABLE_FIRST,            SDRATTR_TABLE_LAST,
                                        SDRATTR_XMLATTRIBUTES,          SDRATTR_XMLATTRIBUTES,

                                        0, 0 };

            pSet = new SfxItemSet(GetPool().GetPool(), nWhichPairTable);
        }

        return *pSet;
    }

    // this is a dummy template for the internal template of the
    // current presentation layout; return the ItemSet of that template
    else
    {

        SdStyleSheet* pSdSheet = GetRealStyleSheet();

        if (pSdSheet)
        {
            return(pSdSheet->GetItemSet());
        }
        else
        {
            if (!pSet)
            {
                sal_uInt16 nWhichPairTable[] = { XATTR_LINE_FIRST,              XATTR_LINE_LAST,
                                             XATTR_FILL_FIRST,              XATTR_FILL_LAST,

                                             SDRATTR_SHADOW_FIRST,          SDRATTR_SHADOW_LAST,
                                             SDRATTR_TEXT_MINFRAMEHEIGHT,   SDRATTR_TEXT_CONTOURFRAME,

                                             SDRATTR_TEXT_WORDWRAP,         SDRATTR_TEXT_AUTOGROWSIZE,

                                             SDRATTR_EDGE_FIRST,            SDRATTR_EDGE_LAST,
                                             SDRATTR_MEASURE_FIRST,         SDRATTR_MEASURE_LAST,

                                             EE_PARA_START,                 EE_CHAR_END,

                                            SDRATTR_XMLATTRIBUTES,          SDRATTR_TEXT_USEFIXEDCELLHEIGHT,

                                            SDRATTR_3D_FIRST, SDRATTR_3D_LAST,
                                             0, 0 };

                pSet = new SfxItemSet(GetPool().GetPool(), nWhichPairTable);
            }

            return(*pSet);
        }
    }
}

/**
 * A template is used when it is referenced by inserted object or by a used
 * template.
 */
bool SdStyleSheet::IsUsed() const
{
    bool bResult = false;

    const size_t nListenerCount = GetSizeOfVector();
    for (size_t n = 0; n < nListenerCount; ++n)
    {
        SfxListener* pListener = GetListener(n);
        if( pListener == this )
            continue;

        const svl::StyleSheetUser* const pUser(dynamic_cast<svl::StyleSheetUser*>(pListener));
        if (pUser)
            bResult = pUser->isUsedByModel();
        if (bResult)
            break;
    }

    if( !bResult )
    {
        MutexGuard aGuard( mrBHelper.rMutex );

        OInterfaceContainerHelper * pContainer = mrBHelper.getContainer( cppu::UnoType<XModifyListener>::get() );
        if( pContainer )
        {
            Sequence< Reference< XInterface > > aModifyListeners( pContainer->getElements() );
            Reference< XInterface > *p = aModifyListeners.getArray();
            sal_Int32 nCount = aModifyListeners.getLength();
            while( nCount-- && !bResult )
            {
                Reference< XStyle > xStyle( *p++, UNO_QUERY );
                if( xStyle.is() )
                    bResult = xStyle->isInUse();
            }
        }
    }
    return bResult;
}

/**
 * Determine the style sheet for which this dummy is for.
 */
SdStyleSheet* SdStyleSheet::GetRealStyleSheet() const
{
    OUString aRealStyle;
    OUString aSep( SD_LT_SEPARATOR );
    SdStyleSheet* pRealStyle = NULL;
    SdDrawDocument* pDoc = ((SdStyleSheetPool*)pPool)->GetDoc();

    ::sd::DrawViewShell* pDrawViewShell = 0;

    ::sd::ViewShellBase* pBase = dynamic_cast< ::sd::ViewShellBase* >( SfxViewShell::Current() );
    if( pBase )
        pDrawViewShell = dynamic_cast< ::sd::DrawViewShell* >( pBase->GetMainViewShell().get() );

    if (pDrawViewShell && pDrawViewShell->GetDoc() == pDoc)
    {
        SdPage* pPage = pDrawViewShell->getCurrentPage();
        if( pPage )
        {
            aRealStyle = pPage->GetLayoutName();
            // cut after separator string

            if( aRealStyle.indexOf(aSep) >= 0)
            {
                aRealStyle = aRealStyle.copy(0,(aRealStyle.indexOf(aSep) + aSep.getLength()));
            }
        }
    }
    if (aRealStyle.isEmpty())
    {
        SdPage* pPage = pDoc->GetSdPage(0, PK_STANDARD);

        if (pPage)
        {
            aRealStyle = pDoc->GetSdPage(0, PK_STANDARD)->GetLayoutName();
        }
        else
        {
            /* no page available yet. This can happen when actualising the
               document templates.  */
            SfxStyleSheetIterator aIter(pPool, SD_STYLE_FAMILY_MASTERPAGE);
            SfxStyleSheetBase* pSheet = aIter.First();
            if( pSheet )
                aRealStyle = pSheet->GetName();
        }

            if( aRealStyle.indexOf(aSep) >= 0)
            {
                aRealStyle = aRealStyle.copy(0,(aRealStyle.indexOf(aSep) + aSep.getLength()));
            }
    }

    /* now map from the name (specified for country language) to the internal
       name (independent of the country language)  */
    OUString aInternalName;
    OUString aStyleName(aName);

    if (aStyleName == OUString(SdResId(STR_PSEUDOSHEET_TITLE)))
    {
        aInternalName = OUString(SdResId(STR_LAYOUT_TITLE));
    }
    else if (aStyleName == OUString(SdResId(STR_PSEUDOSHEET_SUBTITLE)))
    {
        aInternalName = OUString(SdResId(STR_LAYOUT_SUBTITLE));
    }
    else if (aStyleName == OUString(SdResId(STR_PSEUDOSHEET_BACKGROUND)))
    {
        aInternalName = OUString(SdResId(STR_LAYOUT_BACKGROUND));
    }
        else if (aStyleName == OUString(SdResId(STR_PSEUDOSHEET_BACKGROUNDOBJECTS)))
    {
        aInternalName = OUString(SdResId(STR_LAYOUT_BACKGROUNDOBJECTS));
    }
        else if (aStyleName == OUString(SdResId(STR_PSEUDOSHEET_NOTES)))
    {
        aInternalName = OUString(SdResId(STR_LAYOUT_NOTES));
    }
    else
    {
        OUString aOutlineStr(SdResId(STR_PSEUDOSHEET_OUTLINE));
        sal_Int32 nPos = aStyleName.indexOf(aOutlineStr);
        if (nPos >= 0)
        {
            OUString aNumStr(aStyleName.copy(aOutlineStr.getLength()));
            aInternalName = OUString(SdResId(STR_LAYOUT_OUTLINE));
            aInternalName += aNumStr;
        }
    }

    aRealStyle += aInternalName;
    pRealStyle = static_cast< SdStyleSheet* >( pPool->Find(aRealStyle, SD_STYLE_FAMILY_MASTERPAGE) );

#ifdef DBG_UTIL
    if( !pRealStyle )
    {
        SfxStyleSheetIterator aIter(pPool, SD_STYLE_FAMILY_MASTERPAGE);
        if( aIter.Count() > 0 )
            // StyleSheet not found, but pool already loaded
            DBG_ASSERT(pRealStyle, "Internal StyleSheet not found");
    }
#endif

    return pRealStyle;
}

/**
 * Determine pseudo style sheet which stands for this style sheet.
 */
SdStyleSheet* SdStyleSheet::GetPseudoStyleSheet() const
{
    SdStyleSheet* pPseudoStyle = NULL;
    OUString aSep( SD_LT_SEPARATOR );
    OUString aStyleName(aName);
        // without layout name and separator

    if( aStyleName.indexOf(aSep) >=0 )
    {
        aStyleName = aStyleName.copy (aStyleName.indexOf(aSep) + aSep.getLength());
    }

    if (aStyleName == OUString(SdResId(STR_LAYOUT_TITLE)))
    {
        aStyleName = OUString(SdResId(STR_PSEUDOSHEET_TITLE));
    }
    else if (aStyleName == OUString(SdResId(STR_LAYOUT_SUBTITLE)))
    {
        aStyleName = OUString(SdResId(STR_PSEUDOSHEET_SUBTITLE));
    }
    else if (aStyleName == OUString(SdResId(STR_LAYOUT_BACKGROUND)))
    {
        aStyleName = OUString(SdResId(STR_PSEUDOSHEET_BACKGROUND));
    }
    else if (aStyleName == OUString(SdResId(STR_LAYOUT_BACKGROUNDOBJECTS)))
    {
        aStyleName = OUString(SdResId(STR_PSEUDOSHEET_BACKGROUNDOBJECTS));
    }
    else if (aStyleName == OUString(SdResId(STR_LAYOUT_NOTES)))
    {
        aStyleName = OUString(SdResId(STR_PSEUDOSHEET_NOTES));
    }
    else
    {
        OUString aOutlineStr((SdResId(STR_LAYOUT_OUTLINE)));
        sal_Int32 nPos = aStyleName.indexOf(aOutlineStr);
        if (nPos != -1)
        {
            OUString aNumStr(aStyleName.copy(aOutlineStr.getLength()));
            aStyleName = OUString(SdResId(STR_PSEUDOSHEET_OUTLINE));
            aStyleName += aNumStr;
        }
    }

    pPseudoStyle = static_cast<SdStyleSheet*>(pPool->Find(aStyleName, SD_STYLE_FAMILY_PSEUDO));
    DBG_ASSERT(pPseudoStyle, "PseudoStyleSheet missing");

    return pPseudoStyle;
}

void SdStyleSheet::Notify(SfxBroadcaster& rBC, const SfxHint& rHint)
{
    // first, base class functionality
    SfxStyleSheet::Notify(rBC, rHint);

    /* if the dummy gets a notify about a changed attribute, he takes care that
       the actual ment style sheet sends broadcasts. */
    SfxSimpleHint* pSimple = PTR_CAST(SfxSimpleHint, &rHint);
    sal_uLong nId = pSimple == NULL ? 0 : pSimple->GetId();
    if (nId == SFX_HINT_DATACHANGED && nFamily == SD_STYLE_FAMILY_PSEUDO)
    {
        SdStyleSheet* pRealStyle = GetRealStyleSheet();
        if (pRealStyle)
            pRealStyle->Broadcast(rHint);
    }
}

/**
 * Adjust the bullet width and the left text indent of the provided ItemSets to
 * their font height. The new values are calculated that the ratio to the font
 * height is as in the style sheet.
 *
 * @param bOnlyMissingItems If sal_True, only not set items are completed. With
 * sal_False, are items are overwritten.
 */
void SdStyleSheet::AdjustToFontHeight(SfxItemSet& rSet, bool bOnlyMissingItems)
{
    /* If not explicit set, adjust bullet width and text indent to new font
       height. */
    SfxStyleFamily eFamily = nFamily;
    OUString aStyleName(aName);
    if (eFamily == SD_STYLE_FAMILY_PSEUDO)
    {
        SfxStyleSheet* pRealStyle = GetRealStyleSheet();
        eFamily = pRealStyle->GetFamily();
        aStyleName = pRealStyle->GetName();
    }

    if (eFamily == SD_STYLE_FAMILY_MASTERPAGE &&
        aStyleName.indexOf(OUString(SdResId(STR_LAYOUT_OUTLINE))) != -1 &&
        rSet.GetItemState(EE_CHAR_FONTHEIGHT) == SFX_ITEM_SET)
    {
        const SfxItemSet* pCurSet = &GetItemSet();
        sal_uInt32 nNewHeight = ((SvxFontHeightItem&)rSet.Get(EE_CHAR_FONTHEIGHT)).GetHeight();
        sal_uInt32 nOldHeight = ((SvxFontHeightItem&)pCurSet->Get(EE_CHAR_FONTHEIGHT)).GetHeight();

        if (rSet.GetItemState(EE_PARA_BULLET) != SFX_ITEM_SET || !bOnlyMissingItems)
        {
            const SvxBulletItem& rBItem = (const SvxBulletItem&)pCurSet->Get(EE_PARA_BULLET);
            double fBulletFraction = double(rBItem.GetWidth()) / nOldHeight;
            SvxBulletItem aNewBItem(rBItem);
            aNewBItem.SetWidth((sal_uInt32)(fBulletFraction * nNewHeight));
            rSet.Put(aNewBItem);
        }

        if (rSet.GetItemState(EE_PARA_LRSPACE) != SFX_ITEM_SET || !bOnlyMissingItems)
        {
            const SvxLRSpaceItem& rLRItem = (const SvxLRSpaceItem&)pCurSet->Get(EE_PARA_LRSPACE);
            double fIndentFraction = double(rLRItem.GetTxtLeft()) / nOldHeight;
            SvxLRSpaceItem aNewLRItem(rLRItem);
            aNewLRItem.SetTxtLeft(fIndentFraction * nNewHeight);
            double fFirstIndentFraction = double(rLRItem.GetTxtFirstLineOfst()) / nOldHeight;
            aNewLRItem.SetTxtFirstLineOfst((short)(fFirstIndentFraction * nNewHeight));
            rSet.Put(aNewLRItem);
        }

        if (rSet.GetItemState(EE_PARA_ULSPACE) != SFX_ITEM_SET || !bOnlyMissingItems)
        {
            const SvxULSpaceItem& rULItem = (const SvxULSpaceItem&)pCurSet->Get(EE_PARA_ULSPACE);
            SvxULSpaceItem aNewULItem(rULItem);
            double fLowerFraction = double(rULItem.GetLower()) / nOldHeight;
            aNewULItem.SetLower((sal_uInt16)(fLowerFraction * nNewHeight));
            double fUpperFraction = double(rULItem.GetUpper()) / nOldHeight;
            aNewULItem.SetUpper((sal_uInt16)(fUpperFraction * nNewHeight));
            rSet.Put(aNewULItem);
        }
    }
}

bool SdStyleSheet::HasFollowSupport() const
{
    return false;
}

bool SdStyleSheet::HasParentSupport() const
{
    return true;
}

bool SdStyleSheet::HasClearParentSupport() const
{
    return true;
}

bool SdStyleSheet::SetName(const OUString& rName, bool bReindexNow)
{
    return SfxStyleSheet::SetName(rName, bReindexNow);
}

void SdStyleSheet::SetHelpId( const OUString& r, sal_uLong nId )
{
    SfxStyleSheet::SetHelpId( r, nId );

    if( (nId >= HID_PSEUDOSHEET_OUTLINE1) && ( nId <= HID_PSEUDOSHEET_OUTLINE9 ) )
    {
        msApiName = "outline";
        msApiName += OUString( (sal_Unicode)( '1' + (nId - HID_PSEUDOSHEET_OUTLINE1) ) );
    }
    else
    {
        static struct ApiNameMap
        {
            const sal_Char* mpApiName;
            sal_uInt32      mnApiNameLength;
            sal_uInt32      mnHelpId;
        }
        pApiNameMap[] =
        {
            { RTL_CONSTASCII_STRINGPARAM( "title" ),            HID_PSEUDOSHEET_TITLE },
            { RTL_CONSTASCII_STRINGPARAM( "subtitle" ),         HID_PSEUDOSHEET_SUBTITLE },
            { RTL_CONSTASCII_STRINGPARAM( "background" ),       HID_PSEUDOSHEET_BACKGROUND },
            { RTL_CONSTASCII_STRINGPARAM( "backgroundobjects" ),HID_PSEUDOSHEET_BACKGROUNDOBJECTS },
            { RTL_CONSTASCII_STRINGPARAM( "notes" ),            HID_PSEUDOSHEET_NOTES },
            { RTL_CONSTASCII_STRINGPARAM( "standard" ),         HID_STANDARD_STYLESHEET_NAME },
            { RTL_CONSTASCII_STRINGPARAM( "objectwitharrow" ),  HID_POOLSHEET_OBJWITHARROW },
            { RTL_CONSTASCII_STRINGPARAM( "objectwithshadow" ), HID_POOLSHEET_OBJWITHSHADOW },
            { RTL_CONSTASCII_STRINGPARAM( "objectwithoutfill" ),HID_POOLSHEET_OBJWITHOUTFILL },
            { RTL_CONSTASCII_STRINGPARAM( "text" ),             HID_POOLSHEET_TEXT },
            { RTL_CONSTASCII_STRINGPARAM( "textbody" ),         HID_POOLSHEET_TEXTBODY },
            { RTL_CONSTASCII_STRINGPARAM( "textbodyjustfied" ), HID_POOLSHEET_TEXTBODY_JUSTIFY },
            { RTL_CONSTASCII_STRINGPARAM( "textbodyindent" ),   HID_POOLSHEET_TEXTBODY_INDENT },
            { RTL_CONSTASCII_STRINGPARAM( "title" ),            HID_POOLSHEET_TITLE },
            { RTL_CONSTASCII_STRINGPARAM( "title1" ),           HID_POOLSHEET_TITLE1 },
            { RTL_CONSTASCII_STRINGPARAM( "title2" ),           HID_POOLSHEET_TITLE2 },
            { RTL_CONSTASCII_STRINGPARAM( "headline" ),         HID_POOLSHEET_HEADLINE },
            { RTL_CONSTASCII_STRINGPARAM( "headline1" ),        HID_POOLSHEET_HEADLINE1 },
            { RTL_CONSTASCII_STRINGPARAM( "headline2" ),        HID_POOLSHEET_HEADLINE2 },
            { RTL_CONSTASCII_STRINGPARAM( "measure" ),          HID_POOLSHEET_MEASURE },
            { 0, 0, 0 }
        };

        ApiNameMap* p = pApiNameMap;
        while( p->mpApiName )
        {
            if( nId == p->mnHelpId )
            {
                msApiName = OUString( p->mpApiName, p->mnApiNameLength, RTL_TEXTENCODING_ASCII_US );
                break;
            }
            p++;
        }
    }
}

OUString SdStyleSheet::GetFamilyString( SfxStyleFamily eFamily )
{
    switch( eFamily )
    {
    case SD_STYLE_FAMILY_CELL:
        return OUString( "cell" );
    default:
        OSL_FAIL( "SdStyleSheet::GetFamilyString(), illegal family!" );
    case SD_STYLE_FAMILY_GRAPHICS:
        return OUString( "graphics" );
    }
}

void SdStyleSheet::throwIfDisposed() throw (RuntimeException)
{
    if( !mxPool.is() )
        throw DisposedException();
}

SdStyleSheet* SdStyleSheet::CreateEmptyUserStyle( SfxStyleSheetBasePool& rPool, SfxStyleFamily eFamily )
{
    OUString aPrefix( "user" );
    OUString aName;
    sal_Int32 nIndex = 1;
    do
    {
        aName = aPrefix + OUString::number( nIndex++ );
    }
    while( rPool.Find( aName, eFamily ) != 0 );

    return new SdStyleSheet(aName, rPool, eFamily, SFXSTYLEBIT_USERDEF);
}

// XInterface

void SAL_CALL SdStyleSheet::release(  ) throw ()
{
    if (osl_atomic_decrement( &m_refCount ) == 0)
    {
        // restore reference count:
        osl_atomic_increment( &m_refCount );
        if (! mrBHelper.bDisposed) try
        {
            dispose();
        }
        catch (RuntimeException const& exc)
        { // don't break throw ()
            OSL_FAIL(
                OUStringToOString(
                    exc.Message, RTL_TEXTENCODING_ASCII_US ).getStr() );
            static_cast<void>(exc);
        }
        OSL_ASSERT( mrBHelper.bDisposed );
        SdStyleSheetBase::release();
    }
}

// XComponent

void SAL_CALL SdStyleSheet::dispose(  ) throw (RuntimeException, std::exception)
{
    ClearableMutexGuard aGuard( mrBHelper.rMutex );
    if (!mrBHelper.bDisposed && !mrBHelper.bInDispose)
    {
        mrBHelper.bInDispose = sal_True;
        aGuard.clear();
        try
        {
            // side effect: keeping a reference to this
            EventObject aEvt( static_cast< OWeakObject * >( this ) );
            try
            {
                mrBHelper.aLC.disposeAndClear( aEvt );
                disposing();
            }
            catch (...)
            {
                MutexGuard aGuard2( mrBHelper.rMutex );
                // bDisposed and bInDispose must be set in this order:
                mrBHelper.bDisposed = sal_True;
                mrBHelper.bInDispose = sal_False;
                throw;
            }
            MutexGuard aGuard2( mrBHelper.rMutex );
            // bDisposed and bInDispose must be set in this order:
            mrBHelper.bDisposed = sal_True;
            mrBHelper.bInDispose = sal_False;
        }
        catch (RuntimeException &)
        {
            throw;
        }
        catch (const Exception & exc)
        {
            throw RuntimeException( "unexpected UNO exception caught: " + exc.Message );
        }
    }
}

void SdStyleSheet::disposing()
{
    mxPool.clear();
}

void SAL_CALL SdStyleSheet::addEventListener( const Reference< XEventListener >& xListener ) throw (RuntimeException, std::exception)
{
    ClearableMutexGuard aGuard( mrBHelper.rMutex );
    if (mrBHelper.bDisposed || mrBHelper.bInDispose)
    {
        aGuard.clear();
        EventObject aEvt( static_cast< OWeakObject * >( this ) );
        xListener->disposing( aEvt );
    }
    else
    {
        mrBHelper.addListener( ::getCppuType( &xListener ), xListener );
    }
}

void SAL_CALL SdStyleSheet::removeEventListener( const Reference< XEventListener >& xListener  ) throw (RuntimeException, std::exception)
{
    mrBHelper.removeListener( ::getCppuType( &xListener ), xListener );
}

// XModifyBroadcaster

void SAL_CALL SdStyleSheet::addModifyListener( const Reference< XModifyListener >& xListener ) throw (RuntimeException, std::exception)
{
    ClearableMutexGuard aGuard( mrBHelper.rMutex );
    if (mrBHelper.bDisposed || mrBHelper.bInDispose)
    {
        aGuard.clear();
        EventObject aEvt( static_cast< OWeakObject * >( this ) );
        xListener->disposing( aEvt );
    }
    else
    {
        if( !mpModifyListenerForewarder.get() )
            mpModifyListenerForewarder.reset( new ModifyListenerForewarder( this ) );
        mrBHelper.addListener( cppu::UnoType<XModifyListener>::get(), xListener );
    }
}

void SAL_CALL SdStyleSheet::removeModifyListener( const Reference< XModifyListener >& xListener ) throw (RuntimeException, std::exception)
{
    mrBHelper.removeListener( cppu::UnoType<XModifyListener>::get(), xListener );
}

void SdStyleSheet::notifyModifyListener()
{
    MutexGuard aGuard( mrBHelper.rMutex );

    OInterfaceContainerHelper * pContainer = mrBHelper.getContainer( cppu::UnoType<XModifyListener>::get() );
    if( pContainer )
    {
        EventObject aEvt( static_cast< OWeakObject * >( this ) );
        pContainer->forEach<XModifyListener>( boost::bind( &XModifyListener::modified, _1, boost::cref( aEvt ) ) );
    }
}

// XServiceInfo
OUString SAL_CALL SdStyleSheet::getImplementationName() throw(RuntimeException, std::exception)
{
    return OUString( "SdStyleSheet" );
}

sal_Bool SAL_CALL SdStyleSheet::supportsService( const OUString& ServiceName ) throw(RuntimeException, std::exception)
{
    return cppu::supportsService( this, ServiceName );
}

Sequence< OUString > SAL_CALL SdStyleSheet::getSupportedServiceNames() throw(RuntimeException, std::exception)
{
    Sequence< OUString > aNameSequence( 10 );
    OUString* pStrings = aNameSequence.getArray();

    *pStrings++ = "com.sun.star.style.Style";
    *pStrings++ = "com.sun.star.drawing.FillProperties";
    *pStrings++ = "com.sun.star.drawing.LineProperties";
    *pStrings++ = "com.sun.star.drawing.ShadowProperties";
    *pStrings++ = "com.sun.star.drawing.ConnectorProperties";
    *pStrings++ = "com.sun.star.drawing.MeasureProperties";
    *pStrings++ = "com.sun.star.style.ParagraphProperties";
    *pStrings++ = "com.sun.star.style.CharacterProperties";
    *pStrings++ = "com.sun.star.drawing.TextProperties";
    *pStrings++ = "com.sun.star.drawing.Text";

    return aNameSequence;
}

// XNamed
OUString SAL_CALL SdStyleSheet::getName() throw(RuntimeException, std::exception)
{
    SolarMutexGuard aGuard;
    throwIfDisposed();
    return GetApiName();
}

void SAL_CALL SdStyleSheet::setName( const OUString& rName  ) throw(RuntimeException, std::exception)
{
    SolarMutexGuard aGuard;
    throwIfDisposed();

    if( SetName( rName ) )
    {
        msApiName = rName;
        Broadcast(SfxSimpleHint(SFX_HINT_DATACHANGED));
    }
}

// XStyle

sal_Bool SAL_CALL SdStyleSheet::isUserDefined() throw(RuntimeException, std::exception)
{
    SolarMutexGuard aGuard;
    throwIfDisposed();
    return IsUserDefined() ? sal_True : sal_False;
}

sal_Bool SAL_CALL SdStyleSheet::isInUse() throw(RuntimeException, std::exception)
{
    SolarMutexGuard aGuard;
    throwIfDisposed();
    return IsUsed() ? sal_True : sal_False;
}

OUString SAL_CALL SdStyleSheet::getParentStyle() throw(RuntimeException, std::exception)
{
    SolarMutexGuard aGuard;
    throwIfDisposed();

    if( !GetParent().isEmpty() )
    {
        SdStyleSheet* pParentStyle = static_cast< SdStyleSheet* >( mxPool->Find( GetParent(), nFamily ) );
        if( pParentStyle )
            return pParentStyle->msApiName;
    }
    return OUString();
}

void SAL_CALL SdStyleSheet::setParentStyle( const OUString& rParentName  ) throw(NoSuchElementException, RuntimeException, std::exception)
{
    SolarMutexGuard aGuard;
    throwIfDisposed();

    if( !rParentName.isEmpty() )
    {
        boost::shared_ptr<SfxStyleSheetIterator> aSSSI = boost::make_shared<SfxStyleSheetIterator>(mxPool.get(), nFamily);
        for (SfxStyleSheetBase *pStyle = aSSSI->First(); pStyle; pStyle = aSSSI->Next())
        {
            // we hope that we have only sd style sheets
            SdStyleSheet* pSdStyleSheet = static_cast<SdStyleSheet*>(pStyle);
            if (pSdStyleSheet->msApiName == rParentName)
            {
                if( pStyle != this )
                {
                    SetParent( pStyle->GetName() );
                }
                return;
            }
        }
        throw NoSuchElementException();
    }
    else
    {
        SetParent( rParentName );
    }
}

// XPropertySet

Reference< XPropertySetInfo > SdStyleSheet::getPropertySetInfo() throw(RuntimeException, std::exception)
{
    throwIfDisposed();
    static Reference< XPropertySetInfo > xInfo;
    if( !xInfo.is() )
        xInfo = GetStylePropertySet().getPropertySetInfo();
    return xInfo;
}

void SAL_CALL SdStyleSheet::setPropertyValue( const OUString& aPropertyName, const Any& aValue ) throw(UnknownPropertyException, PropertyVetoException, IllegalArgumentException, WrappedTargetException, RuntimeException, std::exception)
{
    SolarMutexGuard aGuard;
    throwIfDisposed();

    const SfxItemPropertySimpleEntry* pEntry = getPropertyMapEntry( aPropertyName );
    if( pEntry == NULL )
    {
        throw UnknownPropertyException();
    }
    else
    {
        if( pEntry->nWID == WID_STYLE_HIDDEN )
        {
            bool bValue = false;
            if ( aValue >>= bValue )
                SetHidden( bValue );
            return;
        }
        if( pEntry->nWID == SDRATTR_TEXTDIRECTION )
            return; // not yet implemented for styles

        if( pEntry->nWID == WID_STYLE_FAMILY )
            throw PropertyVetoException();

        if( (pEntry->nWID == EE_PARA_NUMBULLET) && (GetFamily() == SD_STYLE_FAMILY_MASTERPAGE) )
        {
            OUString aStr;
            const sal_uInt32 nTempHelpId = GetHelpId( aStr );

            if( (nTempHelpId >= HID_PSEUDOSHEET_OUTLINE2) && (nTempHelpId <= HID_PSEUDOSHEET_OUTLINE9) )
                return;
        }

        SfxItemSet &rStyleSet = GetItemSet();

        if( pEntry->nWID == OWN_ATTR_FILLBMP_MODE )
        {
            BitmapMode eMode;
            if( aValue >>= eMode )
            {
                rStyleSet.Put( XFillBmpStretchItem( eMode == BitmapMode_STRETCH ) );
                rStyleSet.Put( XFillBmpTileItem( eMode == BitmapMode_REPEAT ) );
                return;
            }
            throw IllegalArgumentException();
        }

        SfxItemSet aSet( GetPool().GetPool(),   pEntry->nWID, pEntry->nWID);
        aSet.Put( rStyleSet );

        if( !aSet.Count() )
        {
            if( EE_PARA_NUMBULLET == pEntry->nWID )
            {
                Font aBulletFont;
                SdStyleSheetPool::PutNumBulletItem( this, aBulletFont );
                aSet.Put( rStyleSet );
            }
            else
            {
                aSet.Put( GetPool().GetPool().GetDefaultItem( pEntry->nWID ) );
            }
        }

        if( pEntry->nMemberId == MID_NAME &&
            ( pEntry->nWID == XATTR_FILLBITMAP || pEntry->nWID == XATTR_FILLGRADIENT ||
              pEntry->nWID == XATTR_FILLHATCH || pEntry->nWID == XATTR_FILLFLOATTRANSPARENCE ||
              pEntry->nWID == XATTR_LINESTART || pEntry->nWID == XATTR_LINEEND || pEntry->nWID == XATTR_LINEDASH) )
        {
            OUString aTempName;
            if(!(aValue >>= aTempName ))
                throw IllegalArgumentException();

            SvxShape::SetFillAttribute( pEntry->nWID, aTempName, aSet );
        }
        else if(!SvxUnoTextRangeBase::SetPropertyValueHelper( aSet, pEntry, aValue, aSet ))
        {
            SvxItemPropertySet_setPropertyValue( GetStylePropertySet(), pEntry, aValue, aSet );
        }

        rStyleSet.Put( aSet );
        Broadcast(SfxSimpleHint(SFX_HINT_DATACHANGED));
    }
}

Any SAL_CALL SdStyleSheet::getPropertyValue( const OUString& PropertyName ) throw(UnknownPropertyException, WrappedTargetException, RuntimeException, std::exception)
{
    SolarMutexGuard aGuard;

    throwIfDisposed();

    const SfxItemPropertySimpleEntry* pEntry = getPropertyMapEntry( PropertyName );
    if( pEntry == NULL )
    {
        throw UnknownPropertyException();
    }
    else
    {
        Any aAny;

        if( pEntry->nWID == WID_STYLE_FAMILY )
        {
            if( nFamily == SD_STYLE_FAMILY_MASTERPAGE )
            {
                const OUString aLayoutName( GetName() );
                aAny <<= aLayoutName.copy( 0, aLayoutName.indexOf( SD_LT_SEPARATOR) );
            }
            else
            {
                aAny <<= GetFamilyString(nFamily);
            }
        }
        else if( pEntry->nWID == WID_STYLE_DISPNAME )
        {
            aAny <<= maDisplayName;
        }
        else if( pEntry->nWID == SDRATTR_TEXTDIRECTION )
        {
            aAny <<= sal_False;
        }
        else if( pEntry->nWID == OWN_ATTR_FILLBMP_MODE )
        {
            SfxItemSet &rStyleSet = GetItemSet();

            XFillBmpStretchItem* pStretchItem = (XFillBmpStretchItem*)rStyleSet.GetItem(XATTR_FILLBMP_STRETCH);
            XFillBmpTileItem* pTileItem = (XFillBmpTileItem*)rStyleSet.GetItem(XATTR_FILLBMP_TILE);

            if( pStretchItem && pTileItem )
            {
                if( pTileItem->GetValue() )
                    aAny <<= BitmapMode_REPEAT;
                else if( pStretchItem->GetValue() )
                    aAny <<= BitmapMode_STRETCH;
                else
                    aAny <<= BitmapMode_NO_REPEAT;
            }
        }
        else if( pEntry->nWID == WID_STYLE_HIDDEN )
        {
            aAny <<= IsHidden( );
        }
        else
        {
            SfxItemSet aSet( GetPool().GetPool(),   pEntry->nWID, pEntry->nWID);

            const SfxPoolItem* pItem;
            SfxItemSet& rStyleSet = GetItemSet();

            if( rStyleSet.GetItemState( pEntry->nWID, true, &pItem ) == SFX_ITEM_SET )
                aSet.Put(  *pItem );

            if( !aSet.Count() )
                aSet.Put( GetPool().GetPool().GetDefaultItem( pEntry->nWID ) );

            if(SvxUnoTextRangeBase::GetPropertyValueHelper( aSet, pEntry, aAny ))
                return aAny;

            // Hole Wert aus ItemSet
            aAny = SvxItemPropertySet_getPropertyValue( GetStylePropertySet(),pEntry, aSet );
        }

        if( pEntry->aType != aAny.getValueType() )
        {
            // since the sfx uint16 item now exports a sal_Int32, we may have to fix this here
            if( ( pEntry->aType == ::cppu::UnoType<sal_Int16>::get()) && aAny.getValueType() == ::cppu::UnoType<sal_Int32>::get() )
            {
                sal_Int32 nValue = 0;
                aAny >>= nValue;
                aAny <<= (sal_Int16)nValue;
            }
            else
            {
                OSL_FAIL("SvxShape::GetAnyForItem() Returnvalue has wrong Type!" );
            }
        }

        return aAny;
    }
}

void SAL_CALL SdStyleSheet::addPropertyChangeListener( const OUString& , const Reference< XPropertyChangeListener >&  ) throw(UnknownPropertyException, WrappedTargetException, RuntimeException, std::exception) {}
void SAL_CALL SdStyleSheet::removePropertyChangeListener( const OUString& , const Reference< XPropertyChangeListener >&  ) throw(UnknownPropertyException, WrappedTargetException, RuntimeException, std::exception) {}
void SAL_CALL SdStyleSheet::addVetoableChangeListener( const OUString& , const Reference< XVetoableChangeListener >&  ) throw(UnknownPropertyException, WrappedTargetException, RuntimeException, std::exception) {}
void SAL_CALL SdStyleSheet::removeVetoableChangeListener( const OUString& , const Reference< XVetoableChangeListener >&  ) throw(UnknownPropertyException, WrappedTargetException, RuntimeException, std::exception) {}

// XPropertyState

PropertyState SAL_CALL SdStyleSheet::getPropertyState( const OUString& PropertyName ) throw(UnknownPropertyException, RuntimeException, std::exception)
{
    SolarMutexGuard aGuard;

    throwIfDisposed();

    const SfxItemPropertySimpleEntry* pEntry = getPropertyMapEntry( PropertyName );

    if( pEntry == NULL )
        throw UnknownPropertyException();

    if( pEntry->nWID == WID_STYLE_FAMILY )
    {
        return PropertyState_DIRECT_VALUE;
    }
    else if( pEntry->nWID == SDRATTR_TEXTDIRECTION )
    {
        return PropertyState_DEFAULT_VALUE;
    }
    else if( pEntry->nWID == OWN_ATTR_FILLBMP_MODE )
    {
        const SfxItemSet& rSet = GetItemSet();

        if( rSet.GetItemState( XATTR_FILLBMP_STRETCH, false ) == SFX_ITEM_SET ||
            rSet.GetItemState( XATTR_FILLBMP_TILE, false ) == SFX_ITEM_SET )
        {
            return PropertyState_DIRECT_VALUE;
        }
        else
        {
            return PropertyState_AMBIGUOUS_VALUE;
        }
    }
    else
    {
        SfxItemSet &rStyleSet = GetItemSet();

        PropertyState eState;

        switch( rStyleSet.GetItemState( pEntry->nWID, false ) )
        {
        case SFX_ITEM_READONLY:
        case SFX_ITEM_SET:
            eState = PropertyState_DIRECT_VALUE;
            break;
        case SFX_ITEM_DEFAULT:
            eState = PropertyState_DEFAULT_VALUE;
            break;
        default:
            eState = PropertyState_AMBIGUOUS_VALUE;
            break;
        }

        // if a item is set, this doesn't mean we want it :)
        if( ( PropertyState_DIRECT_VALUE == eState ) )
        {
            switch( pEntry->nWID )
            {
            case XATTR_FILLBITMAP:
            case XATTR_FILLGRADIENT:
            case XATTR_FILLHATCH:
            case XATTR_FILLFLOATTRANSPARENCE:
            case XATTR_LINEEND:
            case XATTR_LINESTART:
            case XATTR_LINEDASH:
                {
                    NameOrIndex* pItem = (NameOrIndex*)rStyleSet.GetItem((sal_uInt16)pEntry->nWID);
                    if( ( pItem == NULL ) || pItem->GetName().isEmpty() )
                        eState = PropertyState_DEFAULT_VALUE;
                }
            }
        }

        return eState;
    }
}

Sequence< PropertyState > SAL_CALL SdStyleSheet::getPropertyStates( const Sequence< OUString >& aPropertyName ) throw(UnknownPropertyException, RuntimeException, std::exception)
{
    SolarMutexGuard aGuard;

    throwIfDisposed();

    sal_Int32 nCount = aPropertyName.getLength();
    const OUString* pNames = aPropertyName.getConstArray();

    Sequence< PropertyState > aPropertyStateSequence( nCount );
    PropertyState* pState = aPropertyStateSequence.getArray();

    while( nCount-- )
        *pState++ = getPropertyState( *pNames++ );

    return aPropertyStateSequence;
}

void SAL_CALL SdStyleSheet::setPropertyToDefault( const OUString& PropertyName ) throw(UnknownPropertyException, RuntimeException, std::exception)
{
    SolarMutexGuard aGuard;

    throwIfDisposed();

    const SfxItemPropertySimpleEntry* pEntry = getPropertyMapEntry( PropertyName );
    if( pEntry == NULL )
        throw UnknownPropertyException();

    SfxItemSet &rStyleSet = GetItemSet();

    if( pEntry->nWID == OWN_ATTR_FILLBMP_MODE )
    {
        rStyleSet.ClearItem( XATTR_FILLBMP_STRETCH );
        rStyleSet.ClearItem( XATTR_FILLBMP_TILE );
    }
    else
    {
        rStyleSet.ClearItem( pEntry->nWID );
    }
    Broadcast(SfxSimpleHint(SFX_HINT_DATACHANGED));
}

Any SAL_CALL SdStyleSheet::getPropertyDefault( const OUString& aPropertyName ) throw(UnknownPropertyException, WrappedTargetException, RuntimeException, std::exception)
{
    SolarMutexGuard aGuard;

    throwIfDisposed();

    const SfxItemPropertySimpleEntry* pEntry = getPropertyMapEntry( aPropertyName );
    if( pEntry == NULL )
        throw UnknownPropertyException();
    Any aRet;
    if( pEntry->nWID == WID_STYLE_FAMILY )
    {
        aRet <<= GetFamilyString(nFamily);
    }
    else if( pEntry->nWID == SDRATTR_TEXTDIRECTION )
    {
        aRet <<= sal_False;
    }
    else if( pEntry->nWID == OWN_ATTR_FILLBMP_MODE )
    {
        aRet <<= BitmapMode_REPEAT;
    }
    else
    {
        SfxItemPool& rMyPool = GetPool().GetPool();
        SfxItemSet aSet( rMyPool,   pEntry->nWID, pEntry->nWID);
        aSet.Put( rMyPool.GetDefaultItem( pEntry->nWID ) );
        aRet = SvxItemPropertySet_getPropertyValue( GetStylePropertySet(), pEntry, aSet );
    }
    return aRet;
}

/** this is used because our property map is not sorted yet */
const SfxItemPropertySimpleEntry* SdStyleSheet::getPropertyMapEntry( const OUString& rPropertyName ) const throw (css::uno::RuntimeException)
{
    return GetStylePropertySet().getPropertyMapEntry(rPropertyName);
}

//Broadcast that a SdStyleSheet has changed, taking into account outline sublevels
//which need to be explicitly broadcast as changing if their parent style was
//the one that changed
void SdStyleSheet::BroadcastSdStyleSheetChange(SfxStyleSheetBase* pStyleSheet,
    PresentationObjects ePO, SfxStyleSheetBasePool* pSSPool)
{
    SdStyleSheet* pRealSheet =((SdStyleSheet*)pStyleSheet)->GetRealStyleSheet();
    pRealSheet->Broadcast(SfxSimpleHint(SFX_HINT_DATACHANGED));

    if( (ePO >= PO_OUTLINE_1) && (ePO <= PO_OUTLINE_8) )
    {
        OUString sStyleName(SD_RESSTR(STR_PSEUDOSHEET_OUTLINE) + " ");

        for( sal_uInt16 n = (sal_uInt16)(ePO - PO_OUTLINE_1 + 2); n < 10; n++ )
        {
            OUString aName( sStyleName + OUString::number(n) );

            SfxStyleSheetBase* pSheet = pSSPool->Find( aName, SD_STYLE_FAMILY_PSEUDO);

            if(pSheet)
            {
                SdStyleSheet* pRealStyleSheet = ((SdStyleSheet*)pSheet)->GetRealStyleSheet();
                pRealStyleSheet->Broadcast(SfxSimpleHint(SFX_HINT_DATACHANGED));
            }
        }
    }
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
