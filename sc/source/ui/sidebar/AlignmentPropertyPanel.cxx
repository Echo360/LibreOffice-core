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

#include "AlignmentPropertyPanel.hxx"
#include <editeng/justifyitem.hxx>
#include "sc.hrc"
#include "scresid.hxx"
#include <sfx2/bindings.hxx>
#include <sfx2/dispatch.hxx>
#include <svx/algitem.hxx>
#include <svx/dlgutil.hxx>
#include <vcl/toolbox.hxx>
#include <svx/sidebar/SidebarDialControl.hxx>

using namespace css;
using namespace cssu;

// namespace open

namespace sc { namespace sidebar {

AlignmentPropertyPanel::AlignmentPropertyPanel(
    Window* pParent,
    const cssu::Reference<css::frame::XFrame>& rxFrame,
    SfxBindings* pBindings)
    : PanelLayout(pParent, "AlignmentPropertyPanel", "modules/scalc/ui/sidebaralignment.ui", rxFrame),
      maAlignHorControl(SID_H_ALIGNCELL, *pBindings, *this),
      maLeftIndentControl(SID_ATTR_ALIGN_INDENT, *pBindings, *this),
      maMergeCellControl(FID_MERGE_TOGGLE, *pBindings, *this),
      maWrapTextControl(SID_ATTR_ALIGN_LINEBREAK, *pBindings, *this),
      maAngleControl(SID_ATTR_ALIGN_DEGREES, *pBindings, *this),
      maStackControl(SID_ATTR_ALIGN_STACKED, *pBindings, *this),
      mbMultiDisable(false),
      mxFrame(rxFrame),
      maContext(),
      mpBindings(pBindings)
{
    get(mpFTLeftIndent, "leftindentlabel");
    get(mpMFLeftIndent, "leftindent");
    get(mpCBXWrapText, "wraptext");
    get(mpCBXMergeCell, "mergecells");
    get(mpFtRotate, "orientationlabel");
    get(mpCtrlDial, "orientationcontrol");
    get(mpMtrAngle, "orientationdegrees");
    get(mpCbStacked, "verticallystacked");

    Initialize();

    mpFTLeftIndent->SetBackground(Wallpaper());
    mpFtRotate->SetBackground(Wallpaper());
}

AlignmentPropertyPanel::~AlignmentPropertyPanel()
{
}

void AlignmentPropertyPanel::Initialize()
{
    mpFTLeftIndent->Disable();
    mpMFLeftIndent->Disable();
    mpMFLeftIndent->SetAccessibleName(OUString( "Left Indent"));    //wj acc
    Link aLink = LINK(this, AlignmentPropertyPanel, MFLeftIndentMdyHdl);
    mpMFLeftIndent->SetModifyHdl ( aLink );

    aLink = LINK(this, AlignmentPropertyPanel, CBOXMergnCellClkHdl);
    mpCBXMergeCell->SetClickHdl ( aLink );

    aLink = LINK(this, AlignmentPropertyPanel, CBOXWrapTextClkHdl);
    mpCBXWrapText->SetClickHdl ( aLink );

    //rotation control
    mpCtrlDial->SetAccessibleName(OUString( "Text Orientation"));   //wj acc
    mpCtrlDial->SetModifyHdl(LINK( this, AlignmentPropertyPanel, RotationHdl));

    //rotation
    mpMtrAngle->SetAccessibleName(OUString( "Text Orientation"));   //wj acc
    mpMtrAngle->SetModifyHdl(LINK( this, AlignmentPropertyPanel, AngleModifiedHdl));
    mpMtrAngle->EnableAutocomplete( false );

    //Vertical stacked
    mpCbStacked->SetClickHdl( LINK( this, AlignmentPropertyPanel, ClickStackHdl ) );

    mpMtrAngle->InsertValue(0, FUNIT_CUSTOM);
    mpMtrAngle->InsertValue(45, FUNIT_CUSTOM);
    mpMtrAngle->InsertValue(90, FUNIT_CUSTOM);
    mpMtrAngle->InsertValue(135, FUNIT_CUSTOM);
    mpMtrAngle->InsertValue(180, FUNIT_CUSTOM);
    mpMtrAngle->InsertValue(225, FUNIT_CUSTOM);
    mpMtrAngle->InsertValue(270, FUNIT_CUSTOM);
    mpMtrAngle->InsertValue(315, FUNIT_CUSTOM);
    mpMtrAngle->SetDropDownLineCount(mpMtrAngle->GetEntryCount());

    mpMFLeftIndent->SetAccessibleRelationLabeledBy(mpFTLeftIndent);
    mpMtrAngle->SetAccessibleRelationLabeledBy(mpFtRotate);
}

IMPL_LINK( AlignmentPropertyPanel, AngleModifiedHdl, void *, EMPTYARG )
{
    OUString sTmp = mpMtrAngle->GetText();

    sal_Unicode nChar = sTmp.isEmpty() ? 0 : sTmp[0];
    if((sTmp.getLength()== 1 &&  nChar == '-') ||
        (nChar != '-' && ((nChar < '0') || (nChar > '9') ) ))   ////modify
        return 0;

    double dTmp = sTmp.toDouble();
    FormatDegrees(dTmp);

    sal_Int64 nTmp = (sal_Int64)dTmp*100;
    SfxInt32Item aAngleItem( SID_ATTR_ALIGN_DEGREES,(sal_uInt32) nTmp);

    GetBindings()->GetDispatcher()->Execute(
        SID_ATTR_ALIGN_DEGREES, SFX_CALLMODE_RECORD, &aAngleItem, 0L );
    return 0;
}

IMPL_LINK( AlignmentPropertyPanel, RotationHdl, void *, EMPTYARG )
{
    sal_Int32 nTmp = mpCtrlDial->GetRotation();
    SfxInt32Item aAngleItem( SID_ATTR_ALIGN_DEGREES,(sal_uInt32) nTmp);

    GetBindings()->GetDispatcher()->Execute(
        SID_ATTR_ALIGN_DEGREES, SFX_CALLMODE_RECORD, &aAngleItem, 0L );

    return 0;
}

IMPL_LINK( AlignmentPropertyPanel, ClickStackHdl, void *, EMPTYARG )
{
    bool bVertical = mpCbStacked->IsChecked();
    SfxBoolItem  aStackItem( SID_ATTR_ALIGN_STACKED, bVertical );
    GetBindings()->GetDispatcher()->Execute(
        SID_ATTR_ALIGN_STACKED, SFX_CALLMODE_RECORD, &aStackItem, 0L );
    return 0;
}

IMPL_LINK(AlignmentPropertyPanel, MFLeftIndentMdyHdl, void*, EMPTYARG)
{
    mpCBXWrapText->EnableTriState(false);
    sal_uInt16 nVal = (sal_uInt16)mpMFLeftIndent->GetValue();
    SfxUInt16Item aItem( SID_ATTR_ALIGN_INDENT,  (sal_uInt16)CalcToUnit( nVal,  SFX_MAPUNIT_TWIP ) );

    GetBindings()->GetDispatcher()->Execute(SID_ATTR_ALIGN_INDENT, SFX_CALLMODE_RECORD, &aItem, 0L);
    return( 0L );
}

IMPL_LINK(AlignmentPropertyPanel, CBOXMergnCellClkHdl, void*, EMPTYARG)
{
    bool bState = mpCBXMergeCell->IsChecked();

    //Modified
    //SfxBoolItem aItem( FID_MERGE_TOGGLE , bState);
    //GetBindings()->GetDispatcher()->Execute(FID_MERGE_TOGGLE, SFX_CALLMODE_RECORD, &aItem, false, 0L);
    if(bState)
        GetBindings()->GetDispatcher()->Execute(FID_MERGE_ON, SFX_CALLMODE_RECORD);
    else
        GetBindings()->GetDispatcher()->Execute(FID_MERGE_OFF, SFX_CALLMODE_RECORD);
    GetBindings()->Invalidate(FID_MERGE_TOGGLE,true,false);
    //modified end

    return 0;
}

IMPL_LINK(AlignmentPropertyPanel, CBOXWrapTextClkHdl, void*, EMPTYARG)
{
    bool bState = mpCBXWrapText->IsChecked();
    SfxBoolItem aItem( SID_ATTR_ALIGN_LINEBREAK , bState);
    GetBindings()->GetDispatcher()->Execute(SID_ATTR_ALIGN_LINEBREAK, SFX_CALLMODE_RECORD, &aItem, 0L);
    return 0;
}

AlignmentPropertyPanel* AlignmentPropertyPanel::Create (
    Window* pParent,
    const cssu::Reference<css::frame::XFrame>& rxFrame,
    SfxBindings* pBindings)
{
    if (pParent == NULL)
        throw lang::IllegalArgumentException("no parent Window given to AlignmentPropertyPanel::Create", NULL, 0);
    if ( ! rxFrame.is())
        throw lang::IllegalArgumentException("no XFrame given to AlignmentPropertyPanel::Create", NULL, 1);
    if (pBindings == NULL)
        throw lang::IllegalArgumentException("no SfxBindings given to AlignmentPropertyPanel::Create", NULL, 2);

    return new AlignmentPropertyPanel(
        pParent,
        rxFrame,
        pBindings);
}

void AlignmentPropertyPanel::DataChanged(
    const DataChangedEvent& rEvent)
{
    (void)rEvent;
}

void AlignmentPropertyPanel::HandleContextChange(
    const ::sfx2::sidebar::EnumContext aContext)
{
    if(maContext == aContext)
    {
        // Nothing to do.
        return;
    }

    maContext = aContext;

    // todo
}

void AlignmentPropertyPanel::NotifyItemUpdate(
    sal_uInt16 nSID,
    SfxItemState eState,
    const SfxPoolItem* pState,
    const bool bIsEnabled)
{
    (void)bIsEnabled;

    switch(nSID)
    {
    case SID_H_ALIGNCELL:
        {
            SvxCellHorJustify meHorAlignState = SVX_HOR_JUSTIFY_STANDARD;
            if(eState >= SFX_ITEM_DEFAULT && pState && pState->ISA(SvxHorJustifyItem) )
            {
                const SvxHorJustifyItem* pItem = (const SvxHorJustifyItem*)pState;
                meHorAlignState = (SvxCellHorJustify)pItem->GetValue();
            }

            if( meHorAlignState == SVX_HOR_JUSTIFY_REPEAT )
            {
                mpFtRotate->Disable();
                mpCtrlDial->Disable();
                mpMtrAngle->Disable();
            }
            else
            {
                mpFtRotate->Enable(!mbMultiDisable);
                mpCtrlDial->Enable(!mbMultiDisable);
                mpMtrAngle->Enable(!mbMultiDisable);
            }

            mpCbStacked->Enable( meHorAlignState != SVX_HOR_JUSTIFY_REPEAT );
            mpFTLeftIndent->Enable( meHorAlignState == SVX_HOR_JUSTIFY_LEFT );
            mpMFLeftIndent->Enable( meHorAlignState == SVX_HOR_JUSTIFY_LEFT );
        }
        break;
    case SID_ATTR_ALIGN_INDENT:
        if(eState >= SFX_ITEM_DEFAULT && pState && pState->ISA(SfxUInt16Item) )
        {
                const SfxUInt16Item* pItem = (const SfxUInt16Item*)pState;
                sal_uInt16 nVal = pItem->GetValue();
                mpMFLeftIndent->SetValue( CalcToPoint(nVal, SFX_MAPUNIT_TWIP, 1) );
        }
        else
        {
            mpMFLeftIndent->SetValue(0);
            mpMFLeftIndent->SetText(OUString());
        }
        break;
    case FID_MERGE_TOGGLE:
        if(eState >= SFX_ITEM_DEFAULT && pState && pState->ISA(SfxBoolItem) )
        {
            mpCBXMergeCell->Enable();
            const SfxBoolItem* pItem = (const SfxBoolItem*)pState;
            mpCBXMergeCell->Check(pItem->GetValue());
        }
        else
        {
            mpCBXMergeCell->Check(false);
            mpCBXMergeCell->Disable();
        }
        break;

    case SID_ATTR_ALIGN_LINEBREAK:
        if(eState == SFX_ITEM_DISABLED)
        {
            mpCBXWrapText->EnableTriState(false);
            mpCBXWrapText->Check(false);
            mpCBXWrapText->Disable();
        }
        else
        {
            mpCBXWrapText->Enable();
            if(eState >= SFX_ITEM_DEFAULT && pState && pState->ISA(SfxBoolItem) )
            {
                mpCBXWrapText->EnableTriState(false);
                const SfxBoolItem* pItem = (const SfxBoolItem*)pState;
                mpCBXWrapText->Check(pItem->GetValue());
            }
            else if(eState == SFX_ITEM_DONTCARE)
            {
                mpCBXWrapText->EnableTriState(true);
                mpCBXWrapText->SetState(TRISTATE_INDET);
            }
        }
        break;
    case SID_ATTR_ALIGN_DEGREES:
        if (eState >= SFX_ITEM_AVAILABLE)
        {
            long nTmp = ((const SfxInt32Item*)pState)->GetValue();
            mpMtrAngle->SetValue( nTmp / 100);  //wj
            mpCtrlDial->SetRotation( nTmp );
            switch(nTmp)
            {
                case 0:
                    mpMtrAngle->SelectEntryPos(0);
                break;
                case 4500:
                    mpMtrAngle->SelectEntryPos(1);
                break;
                case 9000:
                    mpMtrAngle->SelectEntryPos(2);
                break;
                case 13500:
                    mpMtrAngle->SelectEntryPos(3);
                break;
                case 18000:
                    mpMtrAngle->SelectEntryPos(4);
                break;
                case 22500:
                    mpMtrAngle->SelectEntryPos(5);
                break;
                case 27000:
                    mpMtrAngle->SelectEntryPos(6);
                break;
                case 31500:
                    mpMtrAngle->SelectEntryPos(7);
            }
        }
        else
        {
            mpMtrAngle->SetText( OUString() );
            mpCtrlDial->SetRotation( 0 );
        }
        break;
    case SID_ATTR_ALIGN_STACKED:
        if (eState >= SFX_ITEM_AVAILABLE)
        {
            mpCbStacked->EnableTriState(false);
            const SfxBoolItem* aStackItem = (const SfxBoolItem*)pState;
            mbMultiDisable = aStackItem->GetValue();
            mpCbStacked->Check(mbMultiDisable);
            mpFtRotate->Enable(!mbMultiDisable);
            mpMtrAngle->Enable(!mbMultiDisable);
            mpCtrlDial->Enable(!mbMultiDisable);
        }
        else
        {
            mbMultiDisable = true;
            mpFtRotate->Disable();
            mpMtrAngle->Disable();
            mpCtrlDial->Disable();
            mpCbStacked->EnableTriState(true);
            mpCbStacked->SetState(TRISTATE_INDET);
        }
    }
}

void AlignmentPropertyPanel::FormatDegrees(double& dTmp)
{
    while(dTmp<0)
        dTmp += 360;
    while (dTmp > 359)  //modify
        dTmp = 359;
}

// namespace close

}} // end of namespace ::sc::sidebar

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
