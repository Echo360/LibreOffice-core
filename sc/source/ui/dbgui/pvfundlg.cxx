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

#undef SC_DLLIMPLEMENTATION

#include "pvfundlg.hxx"

#include <com/sun/star/sheet/DataPilotFieldReferenceType.hpp>
#include <com/sun/star/sheet/DataPilotFieldReferenceItemType.hpp>
#include <com/sun/star/sheet/DataPilotFieldLayoutMode.hpp>
#include <com/sun/star/sheet/DataPilotFieldSortMode.hpp>
#include <com/sun/star/sheet/DataPilotFieldShowItemsMode.hpp>

#include <tools/resary.hxx>
#include <vcl/builder.hxx>
#include <vcl/msgbox.hxx>

#include "scresid.hxx"
#include "dpobject.hxx"
#include "dpsave.hxx"
#include "sc.hrc"
#include "globstr.hrc"
#include "dputil.hxx"

#include <vector>
#include <boost/scoped_ptr.hpp>

using namespace ::com::sun::star::sheet;

using ::com::sun::star::uno::Sequence;
using ::std::vector;

namespace {

/** Appends all strings from the Sequence to the list box.

    Empty strings are replaced by a localized "(empty)" entry and inserted at
    the specified position.

    @return  true = The passed string list contains an empty string entry.
 */
template< typename ListBoxType >
bool lclFillListBox( ListBoxType& rLBox, const Sequence< OUString >& rStrings, sal_Int32 nEmptyPos = LISTBOX_APPEND )
{
    bool bEmpty = false;
    const OUString* pStr = rStrings.getConstArray();
    if( pStr )
    {
        for( const OUString* pEnd = pStr + rStrings.getLength(); pStr != pEnd; ++pStr )
        {
            if( !pStr->isEmpty() )
                rLBox.InsertEntry( *pStr );
            else
            {
                rLBox.InsertEntry( ScGlobal::GetRscString( STR_EMPTYDATA ), nEmptyPos );
                bEmpty = true;
            }
        }
    }
    return bEmpty;
}

template< typename ListBoxType >
bool lclFillListBox( ListBoxType& rLBox, const vector<ScDPLabelData::Member>& rMembers, sal_Int32 nEmptyPos = LISTBOX_APPEND )
{
    bool bEmpty = false;
    vector<ScDPLabelData::Member>::const_iterator itr = rMembers.begin(), itrEnd = rMembers.end();
    for (; itr != itrEnd; ++itr)
    {
        OUString aName = itr->getDisplayName();
        if (!aName.isEmpty())
            rLBox.InsertEntry(aName);
        else
        {
            rLBox.InsertEntry(ScGlobal::GetRscString(STR_EMPTYDATA), nEmptyPos);
            bEmpty = true;
        }
    }
    return bEmpty;
}

/** This table represents the order of the strings in the resource string array. */
static const sal_uInt16 spnFunctions[] =
{
    PIVOT_FUNC_SUM,
    PIVOT_FUNC_COUNT,
    PIVOT_FUNC_AVERAGE,
    PIVOT_FUNC_MAX,
    PIVOT_FUNC_MIN,
    PIVOT_FUNC_PRODUCT,
    PIVOT_FUNC_COUNT_NUM,
    PIVOT_FUNC_STD_DEV,
    PIVOT_FUNC_STD_DEVP,
    PIVOT_FUNC_STD_VAR,
    PIVOT_FUNC_STD_VARP
};

const sal_uInt16 SC_BASEITEM_PREV_POS = 0;
const sal_uInt16 SC_BASEITEM_NEXT_POS = 1;
const sal_uInt16 SC_BASEITEM_USER_POS = 2;

const sal_uInt16 SC_SORTNAME_POS = 0;
const sal_uInt16 SC_SORTDATA_POS = 1;

const long SC_SHOW_DEFAULT = 10;

static const ScDPListBoxWrapper::MapEntryType spRefTypeMap[] =
{
    { 0,                        DataPilotFieldReferenceType::NONE                       },
    { 1,                        DataPilotFieldReferenceType::ITEM_DIFFERENCE            },
    { 2,                        DataPilotFieldReferenceType::ITEM_PERCENTAGE            },
    { 3,                        DataPilotFieldReferenceType::ITEM_PERCENTAGE_DIFFERENCE },
    { 4,                        DataPilotFieldReferenceType::RUNNING_TOTAL              },
    { 5,                        DataPilotFieldReferenceType::ROW_PERCENTAGE             },
    { 6,                        DataPilotFieldReferenceType::COLUMN_PERCENTAGE          },
    { 7,                        DataPilotFieldReferenceType::TOTAL_PERCENTAGE           },
    { 8,                        DataPilotFieldReferenceType::INDEX                      },
    { WRAPPER_LISTBOX_ENTRY_NOTFOUND,   DataPilotFieldReferenceType::NONE               }
};

static const ScDPListBoxWrapper::MapEntryType spLayoutMap[] =
{
    { 0,                        DataPilotFieldLayoutMode::TABULAR_LAYOUT            },
    { 1,                        DataPilotFieldLayoutMode::OUTLINE_SUBTOTALS_TOP     },
    { 2,                        DataPilotFieldLayoutMode::OUTLINE_SUBTOTALS_BOTTOM  },
    { WRAPPER_LISTBOX_ENTRY_NOTFOUND,   DataPilotFieldLayoutMode::TABULAR_LAYOUT    }
};

static const ScDPListBoxWrapper::MapEntryType spShowFromMap[] =
{
    { 0,                        DataPilotFieldShowItemsMode::FROM_TOP       },
    { 1,                        DataPilotFieldShowItemsMode::FROM_BOTTOM    },
    { WRAPPER_LISTBOX_ENTRY_NOTFOUND,   DataPilotFieldShowItemsMode::FROM_TOP }
};

} // namespace

ScDPFunctionListBox::ScDPFunctionListBox(Window* pParent, WinBits nStyle)
    : ListBox(pParent, nStyle)
{
    FillFunctionNames();
}

extern "C" SAL_DLLPUBLIC_EXPORT Window* SAL_CALL makeScDPFunctionListBox(Window *pParent, VclBuilder::stringmap &rMap)
{
    WinBits nWinStyle = WB_LEFT|WB_VCENTER|WB_3DLOOK|WB_SIMPLEMODE;
    OString sBorder = VclBuilder::extractCustomProperty(rMap);
    if (!sBorder.isEmpty())
        nWinStyle |= WB_BORDER;
    return new ScDPFunctionListBox(pParent, nWinStyle);
}

void ScDPFunctionListBox::SetSelection( sal_uInt16 nFuncMask )
{
    if( (nFuncMask == PIVOT_FUNC_NONE) || (nFuncMask == PIVOT_FUNC_AUTO) )
        SetNoSelection();
    else
        for( sal_Int32 nEntry = 0, nCount = GetEntryCount(); nEntry < nCount; ++nEntry )
            SelectEntryPos( nEntry, (nFuncMask & spnFunctions[ nEntry ]) != 0 );
}

sal_uInt16 ScDPFunctionListBox::GetSelection() const
{
    sal_uInt16 nFuncMask = PIVOT_FUNC_NONE;
    for( sal_Int32 nSel = 0, nCount = GetSelectEntryCount(); nSel < nCount; ++nSel )
        nFuncMask |= spnFunctions[ GetSelectEntryPos( nSel ) ];
    return nFuncMask;
}

void ScDPFunctionListBox::FillFunctionNames()
{
    OSL_ENSURE( !GetEntryCount(), "ScDPMultiFuncListBox::FillFunctionNames - do not add texts to resource" );
    Clear();
    ResStringArray aArr( ScResId( SCSTR_DPFUNCLISTBOX ) );
    for( sal_uInt16 nIndex = 0, nCount = sal::static_int_cast<sal_uInt16>(aArr.Count()); nIndex < nCount; ++nIndex )
        InsertEntry( aArr.GetString( nIndex ) );
}

ScDPFunctionDlg::ScDPFunctionDlg(
        Window* pParent, const ScDPLabelDataVector& rLabelVec,
        const ScDPLabelData& rLabelData, const ScPivotFuncData& rFuncData)
    : ModalDialog(pParent, "DataFieldDialog",
        "modules/scalc/ui/datafielddialog.ui")
    , mrLabelVec(rLabelVec)
    , mbEmptyItem(false)
{
    get(mpFtName, "name");
    get(mpLbType, "type");
    mxLbTypeWrp.reset(new ScDPListBoxWrapper(*mpLbType, spRefTypeMap));
    get(mpLbFunc, "functions");
    mpLbFunc->set_height_request(mpLbFunc->GetTextHeight() * 8);
    get(mpFtBaseField, "basefieldft");
    get(mpLbBaseField, "basefield");
    get(mpFtBaseItem, "baseitemft");
    get(mpLbBaseItem, "baseitem");
    get(mpBtnOk, "ok");

    Init( rLabelData, rFuncData );
}

sal_uInt16 ScDPFunctionDlg::GetFuncMask() const
{
    return mpLbFunc->GetSelection();
}

DataPilotFieldReference ScDPFunctionDlg::GetFieldRef() const
{
    DataPilotFieldReference aRef;

    aRef.ReferenceType = mxLbTypeWrp->GetControlValue();
    aRef.ReferenceField = GetBaseFieldName(mpLbBaseField->GetSelectEntry());

    sal_Int32 nBaseItemPos = mpLbBaseItem->GetSelectEntryPos();
    switch( nBaseItemPos )
    {
        case SC_BASEITEM_PREV_POS:
            aRef.ReferenceItemType = DataPilotFieldReferenceItemType::PREVIOUS;
        break;
        case SC_BASEITEM_NEXT_POS:
            aRef.ReferenceItemType = DataPilotFieldReferenceItemType::NEXT;
        break;
        default:
        {
            aRef.ReferenceItemType = DataPilotFieldReferenceItemType::NAMED;
            if( !mbEmptyItem || (nBaseItemPos > SC_BASEITEM_USER_POS) )
                aRef.ReferenceItemName = GetBaseItemName(mpLbBaseItem->GetSelectEntry());
        }
    }

    return aRef;
}

void ScDPFunctionDlg::Init( const ScDPLabelData& rLabelData, const ScPivotFuncData& rFuncData )
{
    // list box
    sal_uInt16 nFuncMask = (rFuncData.mnFuncMask == PIVOT_FUNC_NONE) ? PIVOT_FUNC_SUM : rFuncData.mnFuncMask;
    mpLbFunc->SetSelection( nFuncMask );

    // field name
    mpFtName->SetText(rLabelData.getDisplayName());

    // handlers
    mpLbFunc->SetDoubleClickHdl( LINK( this, ScDPFunctionDlg, DblClickHdl ) );
    mpLbType->SetSelectHdl( LINK( this, ScDPFunctionDlg, SelectHdl ) );
    mpLbBaseField->SetSelectHdl( LINK( this, ScDPFunctionDlg, SelectHdl ) );

    // base field list box
    OUString aSelectedEntry;
    for( ScDPLabelDataVector::const_iterator aIt = mrLabelVec.begin(), aEnd = mrLabelVec.end(); aIt != aEnd; ++aIt )
    {
        mpLbBaseField->InsertEntry(aIt->getDisplayName());
        maBaseFieldNameMap.insert(
            NameMapType::value_type(aIt->getDisplayName(), aIt->maName));
        if (aIt->maName == rFuncData.maFieldRef.ReferenceField)
            aSelectedEntry = aIt->getDisplayName();
    }

    // base item list box
    mpLbBaseItem->SetSeparatorPos( SC_BASEITEM_USER_POS - 1 );

    // select field reference type
    mxLbTypeWrp->SetControlValue( rFuncData.maFieldRef.ReferenceType );
    SelectHdl( mpLbType );         // enables base field/item list boxes

    // select base field
    mpLbBaseField->SelectEntry(aSelectedEntry);
    if( mpLbBaseField->GetSelectEntryPos() >= mpLbBaseField->GetEntryCount() )
        mpLbBaseField->SelectEntryPos( 0 );
    SelectHdl( mpLbBaseField );    // fills base item list, selects base item

    // select base item
    switch( rFuncData.maFieldRef.ReferenceItemType )
    {
        case DataPilotFieldReferenceItemType::PREVIOUS:
            mpLbBaseItem->SelectEntryPos( SC_BASEITEM_PREV_POS );
        break;
        case DataPilotFieldReferenceItemType::NEXT:
            mpLbBaseItem->SelectEntryPos( SC_BASEITEM_NEXT_POS );
        break;
        default:
        {
            if( mbEmptyItem && rFuncData.maFieldRef.ReferenceItemName.isEmpty() )
            {
                // select special "(empty)" entry added before other items
                mpLbBaseItem->SelectEntryPos( SC_BASEITEM_USER_POS );
            }
            else
            {
                sal_Int32 nStartPos = mbEmptyItem ? (SC_BASEITEM_USER_POS + 1) : SC_BASEITEM_USER_POS;
                sal_Int32 nPos = FindBaseItemPos( rFuncData.maFieldRef.ReferenceItemName, nStartPos );
                if( nPos >= mpLbBaseItem->GetEntryCount() )
                    nPos = (mpLbBaseItem->GetEntryCount() > SC_BASEITEM_USER_POS) ? SC_BASEITEM_USER_POS : SC_BASEITEM_PREV_POS;
                mpLbBaseItem->SelectEntryPos( nPos );
            }
        }
    }
}

const OUString& ScDPFunctionDlg::GetBaseFieldName(const OUString& rLayoutName) const
{
    NameMapType::const_iterator itr = maBaseFieldNameMap.find(rLayoutName);
    return itr == maBaseFieldNameMap.end() ? rLayoutName : itr->second;
}

const OUString& ScDPFunctionDlg::GetBaseItemName(const OUString& rLayoutName) const
{
    NameMapType::const_iterator itr = maBaseItemNameMap.find(rLayoutName);
    return itr == maBaseItemNameMap.end() ? rLayoutName : itr->second;
}

sal_Int32 ScDPFunctionDlg::FindBaseItemPos( const OUString& rEntry, sal_Int32 nStartPos ) const
{
    sal_Int32 nPos = nStartPos;
    bool bFound = false;
    while (nPos < mpLbBaseItem->GetEntryCount())
    {
        // translate the displayed field name back to its original field name.
        const OUString& rName = GetBaseItemName(mpLbBaseItem->GetEntry(nPos));
        if (rName.equals(rEntry))
        {
            bFound = true;
            break;
        }
        ++nPos;
    }
    return bFound ? nPos : LISTBOX_ENTRY_NOTFOUND;
}

IMPL_LINK( ScDPFunctionDlg, SelectHdl, ListBox*, pLBox )
{
    if( pLBox == mpLbType )
    {
        bool bEnableField, bEnableItem;
        switch( mxLbTypeWrp->GetControlValue() )
        {
            case DataPilotFieldReferenceType::ITEM_DIFFERENCE:
            case DataPilotFieldReferenceType::ITEM_PERCENTAGE:
            case DataPilotFieldReferenceType::ITEM_PERCENTAGE_DIFFERENCE:
                bEnableField = bEnableItem = true;
            break;

            case DataPilotFieldReferenceType::RUNNING_TOTAL:
                bEnableField = true;
                bEnableItem = false;
            break;

            default:
                bEnableField = bEnableItem = false;
        }

        bEnableField &= mpLbBaseField->GetEntryCount() > 0;
        mpFtBaseField->Enable( bEnableField );
        mpLbBaseField->Enable( bEnableField );

        bEnableItem &= bEnableField;
        mpFtBaseItem->Enable( bEnableItem );
        mpLbBaseItem->Enable( bEnableItem );
    }
    else if( pLBox == mpLbBaseField )
    {
        // keep "previous" and "next" entries
        while( mpLbBaseItem->GetEntryCount() > SC_BASEITEM_USER_POS )
            mpLbBaseItem->RemoveEntry( SC_BASEITEM_USER_POS );

        // update item list for current base field
        mbEmptyItem = false;
        size_t nBasePos = mpLbBaseField->GetSelectEntryPos();
        if( nBasePos < mrLabelVec.size() )
        {
            const vector<ScDPLabelData::Member>& rMembers = mrLabelVec[nBasePos].maMembers;
            mbEmptyItem = lclFillListBox(*mpLbBaseItem, rMembers, SC_BASEITEM_USER_POS);
            // build cache for base names.
            NameMapType aMap;
            vector<ScDPLabelData::Member>::const_iterator itr = rMembers.begin(), itrEnd = rMembers.end();
            for (; itr != itrEnd; ++itr)
                aMap.insert(NameMapType::value_type(itr->getDisplayName(), itr->maName));
            maBaseItemNameMap.swap(aMap);
        }

        // select base item
        sal_uInt16 nItemPos = (mpLbBaseItem->GetEntryCount() > SC_BASEITEM_USER_POS) ? SC_BASEITEM_USER_POS : SC_BASEITEM_PREV_POS;
        mpLbBaseItem->SelectEntryPos( nItemPos );
    }
    return 0;
}

IMPL_LINK_NOARG(ScDPFunctionDlg, DblClickHdl)
{
    mpBtnOk->Click();
    return 0;
}

ScDPSubtotalDlg::ScDPSubtotalDlg( Window* pParent, ScDPObject& rDPObj,
        const ScDPLabelData& rLabelData, const ScPivotFuncData& rFuncData,
        const ScDPNameVec& rDataFields, bool bEnableLayout )
    : ModalDialog(pParent, "PivotFieldDialog",
        "modules/scalc/ui/pivotfielddialog.ui")
    , mrDPObj(rDPObj)
    , mrDataFields(rDataFields)
    , maLabelData(rLabelData)
    , mbEnableLayout(bEnableLayout)
{
    get(mpBtnOk, "ok");
    get(mpBtnOptions, "options");
    get(mpCbShowAll, "showall");
    get(mpFtName, "name");
    get(mpLbFunc, "functions");
    mpLbFunc->EnableMultiSelection(true);
    mpLbFunc->set_height_request(mpLbFunc->GetTextHeight() * 8);
    get(mpRbNone, "none");
    get(mpRbAuto, "auto");
    get(mpRbUser, "user");

    Init( rLabelData, rFuncData );
}

sal_uInt16 ScDPSubtotalDlg::GetFuncMask() const
{
    sal_uInt16 nFuncMask = PIVOT_FUNC_NONE;

    if( mpRbAuto->IsChecked() )
        nFuncMask = PIVOT_FUNC_AUTO;
    else if( mpRbUser->IsChecked() )
        nFuncMask = mpLbFunc->GetSelection();

    return nFuncMask;
}

void ScDPSubtotalDlg::FillLabelData( ScDPLabelData& rLabelData ) const
{
    rLabelData.mnFuncMask = GetFuncMask();
    rLabelData.mnUsedHier = maLabelData.mnUsedHier;
    rLabelData.mbShowAll = mpCbShowAll->IsChecked();
    rLabelData.maMembers = maLabelData.maMembers;
    rLabelData.maSortInfo = maLabelData.maSortInfo;
    rLabelData.maLayoutInfo = maLabelData.maLayoutInfo;
    rLabelData.maShowInfo = maLabelData.maShowInfo;
}

void ScDPSubtotalDlg::Init( const ScDPLabelData& rLabelData, const ScPivotFuncData& rFuncData )
{
    // field name
    mpFtName->SetText(rLabelData.getDisplayName());

    // radio buttons
    mpRbNone->SetClickHdl( LINK( this, ScDPSubtotalDlg, RadioClickHdl ) );
    mpRbAuto->SetClickHdl( LINK( this, ScDPSubtotalDlg, RadioClickHdl ) );
    mpRbUser->SetClickHdl( LINK( this, ScDPSubtotalDlg, RadioClickHdl ) );

    RadioButton* pRBtn = 0;
    switch( rFuncData.mnFuncMask )
    {
        case PIVOT_FUNC_NONE:   pRBtn = mpRbNone;  break;
        case PIVOT_FUNC_AUTO:   pRBtn = mpRbAuto;  break;
        default:                pRBtn = mpRbUser;
    }
    pRBtn->Check();
    RadioClickHdl( pRBtn );

    // list box
    mpLbFunc->SetSelection( rFuncData.mnFuncMask );
    mpLbFunc->SetDoubleClickHdl( LINK( this, ScDPSubtotalDlg, DblClickHdl ) );

    // show all
    mpCbShowAll->Check( rLabelData.mbShowAll );

    // options
    mpBtnOptions->SetClickHdl( LINK( this, ScDPSubtotalDlg, ClickHdl ) );
}

IMPL_LINK( ScDPSubtotalDlg, RadioClickHdl, RadioButton*, pBtn )
{
    mpLbFunc->Enable( pBtn == mpRbUser );
    return 0;
}

IMPL_LINK_NOARG(ScDPSubtotalDlg, DblClickHdl)
{
    mpBtnOk->Click();
    return 0;
}

IMPL_LINK( ScDPSubtotalDlg, ClickHdl, PushButton*, pBtn )
{
    if (pBtn == mpBtnOptions)
    {
        boost::scoped_ptr<ScDPSubtotalOptDlg> pDlg(new ScDPSubtotalOptDlg( this, mrDPObj, maLabelData, mrDataFields, mbEnableLayout ));
        if( pDlg->Execute() == RET_OK )
            pDlg->FillLabelData( maLabelData );
    }
    return 0;
}

ScDPSubtotalOptDlg::ScDPSubtotalOptDlg( Window* pParent, ScDPObject& rDPObj,
        const ScDPLabelData& rLabelData, const ScDPNameVec& rDataFields,
        bool bEnableLayout )
    : ModalDialog(pParent, "DataFieldOptionsDialog",
        "modules/scalc/ui/datafieldoptionsdialog.ui")
    , mrDPObj(rDPObj)
    , maLabelData(rLabelData)
{
    get(m_pLbSortBy, "sortby");
    m_pLbSortBy->set_width_request(m_pLbSortBy->approximate_char_width() * 20);
    get(m_pRbSortAsc, "ascending");
    get(m_pRbSortDesc, "descending");
    get(m_pRbSortMan, "manual");
    get(m_pLayoutFrame, "layoutframe");
    get(m_pLbLayout, "layout");
    get(m_pCbLayoutEmpty, "emptyline");
    get(m_pCbShow, "show");
    get(m_pNfShow, "items");
    get(m_pFtShow, "showft");
    get(m_pFtShowFrom, "showfromft");
    get(m_pLbShowFrom, "from");
    get(m_pFtShowUsing, "usingft");
    get(m_pLbShowUsing, "using");
    get(m_pHideFrame, "hideframe");
    get(m_pLbHide, "hideitems");
    m_pLbHide->set_height_request(GetTextHeight() * 5);
    get(m_pFtHierarchy, "hierarchyft");
    get(m_pLbHierarchy, "hierarchy");

    m_xLbLayoutWrp.reset(new ScDPListBoxWrapper(*m_pLbLayout, spLayoutMap));
    m_xLbShowFromWrp.reset(new ScDPListBoxWrapper(*m_pLbShowFrom, spShowFromMap));

    Init( rDataFields, bEnableLayout );
}

void ScDPSubtotalOptDlg::FillLabelData( ScDPLabelData& rLabelData ) const
{
    // *** SORTING ***

    if( m_pRbSortMan->IsChecked() )
        rLabelData.maSortInfo.Mode = DataPilotFieldSortMode::MANUAL;
    else if( m_pLbSortBy->GetSelectEntryPos() == SC_SORTNAME_POS )
        rLabelData.maSortInfo.Mode = DataPilotFieldSortMode::NAME;
    else
        rLabelData.maSortInfo.Mode = DataPilotFieldSortMode::DATA;

    ScDPName aFieldName = GetFieldName(m_pLbSortBy->GetSelectEntry());
    if (!aFieldName.maName.isEmpty())
    {
        rLabelData.maSortInfo.Field =
            ScDPUtil::createDuplicateDimensionName(aFieldName.maName, aFieldName.mnDupCount);
        rLabelData.maSortInfo.IsAscending = m_pRbSortAsc->IsChecked();
    }

    // *** LAYOUT MODE ***

    rLabelData.maLayoutInfo.LayoutMode = m_xLbLayoutWrp->GetControlValue();
    rLabelData.maLayoutInfo.AddEmptyLines = m_pCbLayoutEmpty->IsChecked();

    // *** AUTO SHOW ***

    aFieldName = GetFieldName(m_pLbShowUsing->GetSelectEntry());
    if (!aFieldName.maName.isEmpty())
    {
        rLabelData.maShowInfo.IsEnabled = m_pCbShow->IsChecked();
        rLabelData.maShowInfo.ShowItemsMode = m_xLbShowFromWrp->GetControlValue();
        rLabelData.maShowInfo.ItemCount = sal::static_int_cast<sal_Int32>( m_pNfShow->GetValue() );
        rLabelData.maShowInfo.DataField =
            ScDPUtil::createDuplicateDimensionName(aFieldName.maName, aFieldName.mnDupCount);
    }

    // *** HIDDEN ITEMS ***

    rLabelData.maMembers = maLabelData.maMembers;
    sal_uLong nVisCount = m_pLbHide->GetEntryCount();
    for( sal_uInt16 nPos = 0; nPos < nVisCount; ++nPos )
        rLabelData.maMembers[nPos].mbVisible = !m_pLbHide->IsChecked(nPos);

    // *** HIERARCHY ***

    rLabelData.mnUsedHier = m_pLbHierarchy->GetSelectEntryCount() ? m_pLbHierarchy->GetSelectEntryPos() : 0;
}

void ScDPSubtotalOptDlg::Init( const ScDPNameVec& rDataFields, bool bEnableLayout )
{
    // *** SORTING ***

    sal_Int32 nSortMode = maLabelData.maSortInfo.Mode;

    // sort fields list box
    m_pLbSortBy->InsertEntry(maLabelData.getDisplayName());

    for( ScDPNameVec::const_iterator aIt = rDataFields.begin(), aEnd = rDataFields.end(); aIt != aEnd; ++aIt )
    {
        // Cache names for later lookup.
        maDataFieldNameMap.insert(NameMapType::value_type(aIt->maLayoutName, *aIt));

        m_pLbSortBy->InsertEntry( aIt->maLayoutName );
        m_pLbShowUsing->InsertEntry( aIt->maLayoutName );  // for AutoShow
    }

    if( m_pLbSortBy->GetEntryCount() > SC_SORTDATA_POS )
        m_pLbSortBy->SetSeparatorPos( SC_SORTDATA_POS - 1 );

    sal_Int32 nSortPos = SC_SORTNAME_POS;
    if( nSortMode == DataPilotFieldSortMode::DATA )
    {
        nSortPos = FindListBoxEntry( *m_pLbSortBy, maLabelData.maSortInfo.Field, SC_SORTDATA_POS );
        if( nSortPos >= m_pLbSortBy->GetEntryCount() )
        {
            nSortPos = SC_SORTNAME_POS;
            nSortMode = DataPilotFieldSortMode::MANUAL;
        }
    }
    m_pLbSortBy->SelectEntryPos( nSortPos );

    // sorting mode
    m_pRbSortAsc->SetClickHdl( LINK( this, ScDPSubtotalOptDlg, RadioClickHdl ) );
    m_pRbSortDesc->SetClickHdl( LINK( this, ScDPSubtotalOptDlg, RadioClickHdl ) );
    m_pRbSortMan->SetClickHdl( LINK( this, ScDPSubtotalOptDlg, RadioClickHdl ) );

    RadioButton* pRBtn = 0;
    switch( nSortMode )
    {
        case DataPilotFieldSortMode::NONE:
        case DataPilotFieldSortMode::MANUAL:
            pRBtn = m_pRbSortMan;
        break;
        default:
            pRBtn = maLabelData.maSortInfo.IsAscending ? m_pRbSortAsc : m_pRbSortDesc;
    }
    pRBtn->Check();
    RadioClickHdl( pRBtn );

    // *** LAYOUT MODE ***

    m_pLayoutFrame->Enable(bEnableLayout);

    m_xLbLayoutWrp->SetControlValue( maLabelData.maLayoutInfo.LayoutMode );
    m_pCbLayoutEmpty->Check( maLabelData.maLayoutInfo.AddEmptyLines );

    // *** AUTO SHOW ***

    m_pCbShow->Check( maLabelData.maShowInfo.IsEnabled );
    m_pCbShow->SetClickHdl( LINK( this, ScDPSubtotalOptDlg, CheckHdl ) );

    m_xLbShowFromWrp->SetControlValue( maLabelData.maShowInfo.ShowItemsMode );
    long nCount = static_cast< long >( maLabelData.maShowInfo.ItemCount );
    if( nCount < 1 )
        nCount = SC_SHOW_DEFAULT;
    m_pNfShow->SetValue( nCount );

    // m_pLbShowUsing already filled above
    m_pLbShowUsing->SelectEntry( maLabelData.maShowInfo.DataField );
    if( m_pLbShowUsing->GetSelectEntryPos() >= m_pLbShowUsing->GetEntryCount() )
        m_pLbShowUsing->SelectEntryPos( 0 );

    CheckHdl(m_pCbShow);      // enable/disable dependent controls

    // *** HIDDEN ITEMS ***

    InitHideListBox();

    // *** HIERARCHY ***

    if( maLabelData.maHiers.getLength() > 1 )
    {
        lclFillListBox( *m_pLbHierarchy, maLabelData.maHiers );
        sal_Int32 nHier = maLabelData.mnUsedHier;
        if( (nHier < 0) || (nHier >= maLabelData.maHiers.getLength()) ) nHier = 0;
        m_pLbHierarchy->SelectEntryPos( static_cast< sal_Int32 >( nHier ) );
        m_pLbHierarchy->SetSelectHdl( LINK( this, ScDPSubtotalOptDlg, SelectHdl ) );
    }
    else
    {
        m_pFtHierarchy->Disable();
        m_pLbHierarchy->Disable();
    }
}

void ScDPSubtotalOptDlg::InitHideListBox()
{
    m_pLbHide->Clear();
    lclFillListBox( *m_pLbHide, maLabelData.maMembers );
    size_t n = maLabelData.maMembers.size();
    for (sal_uLong i = 0; i < n; ++i)
        m_pLbHide->CheckEntryPos(i, !maLabelData.maMembers[i].mbVisible);
    bool bEnable = m_pLbHide->GetEntryCount() > 0;
    m_pHideFrame->Enable(bEnable);
}

ScDPName ScDPSubtotalOptDlg::GetFieldName(const OUString& rLayoutName) const
{
    NameMapType::const_iterator itr = maDataFieldNameMap.find(rLayoutName);
    return itr == maDataFieldNameMap.end() ? ScDPName() : itr->second;
}

sal_Int32 ScDPSubtotalOptDlg::FindListBoxEntry(
    const ListBox& rLBox, const OUString& rEntry, sal_Int32 nStartPos ) const
{
    sal_Int32 nPos = nStartPos;
    bool bFound = false;
    while (nPos < rLBox.GetEntryCount())
    {
        // translate the displayed field name back to its original field name.
        ScDPName aName = GetFieldName(rLBox.GetEntry(nPos));
        OUString aUnoName = ScDPUtil::createDuplicateDimensionName(aName.maName, aName.mnDupCount);
        if (aUnoName.equals(rEntry))
        {
            bFound = true;
            break;
        }
        ++nPos;
    }
    return bFound ? nPos : LISTBOX_ENTRY_NOTFOUND;
}

IMPL_LINK( ScDPSubtotalOptDlg, RadioClickHdl, RadioButton*, pBtn )
{
    m_pLbSortBy->Enable( pBtn != m_pRbSortMan );
    return 0;
}

IMPL_LINK( ScDPSubtotalOptDlg, CheckHdl, CheckBox*, pCBox )
{
    if (pCBox == m_pCbShow)
    {
        bool bEnable = m_pCbShow->IsChecked();
        m_pNfShow->Enable( bEnable );
        m_pFtShow->Enable( bEnable );
        m_pFtShowFrom->Enable( bEnable );
        m_pLbShowFrom->Enable( bEnable );

        bool bEnableUsing = bEnable && (m_pLbShowUsing->GetEntryCount() > 0);
        m_pFtShowUsing->Enable(bEnableUsing);
        m_pLbShowUsing->Enable(bEnableUsing);
    }
    return 0;
}

IMPL_LINK( ScDPSubtotalOptDlg, SelectHdl, ListBox*, pLBox )
{
    if (pLBox == m_pLbHierarchy)
    {
        mrDPObj.GetMembers(maLabelData.mnCol, m_pLbHierarchy->GetSelectEntryPos(), maLabelData.maMembers);
        InitHideListBox();
    }
    return 0;
}

ScDPShowDetailDlg::ScDPShowDetailDlg( Window* pParent, ScDPObject& rDPObj, sal_uInt16 nOrient ) :
    ModalDialog     ( pParent, "ShowDetail", "modules/scalc/ui/showdetaildialog.ui" ),
    mrDPObj(rDPObj)
{
    get(mpLbDims, "dimsTreeview");
    get(mpBtnOk, "ok");

    ScDPSaveData* pSaveData = rDPObj.GetSaveData();
    long nDimCount = rDPObj.GetDimCount();
    for (long nDim=0; nDim<nDimCount; nDim++)
    {
        bool bIsDataLayout;
        sal_Int32 nDimFlags = 0;
        OUString aName = rDPObj.GetDimName( nDim, bIsDataLayout, &nDimFlags );
        if ( !bIsDataLayout && !rDPObj.IsDuplicated( nDim ) && ScDPObject::IsOrientationAllowed( nOrient, nDimFlags ) )
        {
            const ScDPSaveDimension* pDimension = pSaveData ? pSaveData->GetExistingDimensionByName(aName) : 0;
            if ( !pDimension || (pDimension->GetOrientation() != nOrient) )
            {
                if (pDimension)
                {
                    const OUString* pLayoutName = pDimension->GetLayoutName();
                    if (pLayoutName)
                        aName = *pLayoutName;
                }
                mpLbDims->InsertEntry( aName );
                maNameIndexMap.insert(DimNameIndexMap::value_type(aName, nDim));
            }
        }
    }
    if( mpLbDims->GetEntryCount() )
        mpLbDims->SelectEntryPos( 0 );

    mpLbDims->SetDoubleClickHdl( LINK( this, ScDPShowDetailDlg, DblClickHdl ) );
}

short ScDPShowDetailDlg::Execute()
{
    return mpLbDims->GetEntryCount() ? ModalDialog::Execute() : static_cast<short>(RET_CANCEL);
}

OUString ScDPShowDetailDlg::GetDimensionName() const
{
    // Look up the internal dimension name which may be different from the
    // displayed field name.
    OUString aSelectedName = mpLbDims->GetSelectEntry();
    DimNameIndexMap::const_iterator itr = maNameIndexMap.find(aSelectedName);
    if (itr == maNameIndexMap.end())
        // This should never happen!
        return aSelectedName;

    long nDim = itr->second;
    bool bIsDataLayout = false;
    return mrDPObj.GetDimName(nDim, bIsDataLayout);
}

IMPL_LINK( ScDPShowDetailDlg, DblClickHdl, ListBox*, pLBox )
{
    if( pLBox == mpLbDims )
        mpBtnOk->Click();
    return 0;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
