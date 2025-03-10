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

#include "tools/rc.h"
#include "tools/debug.hxx"

#include "vcl/decoview.hxx"
#include "vcl/dialog.hxx"
#include "vcl/event.hxx"
#include "vcl/scrbar.hxx"
#include "vcl/button.hxx"
#include "vcl/edit.hxx"
#include "vcl/lstbox.hxx"
#include "vcl/combobox.hxx"
#include "vcl/settings.hxx"

#include "svdata.hxx"
#include "controldata.hxx"
#include "ilstbox.hxx"
#include "dndevdis.hxx"

#include <com/sun/star/datatransfer/dnd/XDropTarget.hpp>

void ListBox::EnableQuickSelection( const bool& b )
{
    mpImplLB->GetMainWindow().EnableQuickSelection(b);
}

ListBox::ListBox(WindowType nType)
    : Control(nType)
    , mpImplLB(NULL)
{
    ImplInitListBoxData();
}

ListBox::ListBox( Window* pParent, WinBits nStyle ) : Control( WINDOW_LISTBOX )
{
    ImplInitListBoxData();
    ImplInit( pParent, nStyle );
}

ListBox::ListBox( Window* pParent, const ResId& rResId ) :
    Control( WINDOW_LISTBOX )
{
    rResId.SetRT( RSC_LISTBOX );
    WinBits nStyle = ImplInitRes( rResId );
    ImplInitListBoxData();
    ImplInit( pParent, nStyle );
    ImplLoadRes( rResId );

    if ( !(nStyle & WB_HIDE ) )
        Show();
}

ListBox::~ListBox()
{
    ImplCallEventListeners( VCLEVENT_OBJECT_DYING );

    // When destroying the FloatWin TH does a GrabFocus to the Parent:
    // that means this "ListBox => PreNotify() ..."
    ImplListBox *pImplLB = mpImplLB;
    mpImplLB = NULL;
    delete pImplLB;

    delete mpFloatWin;
    delete mpImplWin;
    delete mpBtn;
}

void ListBox::ImplInitListBoxData()
{
    mpFloatWin      = NULL;
    mpImplWin       = NULL;
    mpBtn           = NULL;
    mnDDHeight      = 0;
    mnSaveValue     = LISTBOX_ENTRY_NOTFOUND;
    mnLineCount     = 0;
    m_nMaxWidthChars = -1;
    mbDDAutoSize    = true;
    mbEdgeBlending  = false;
}

void ListBox::ImplInit( Window* pParent, WinBits nStyle )
{
    nStyle = ImplInitStyle( nStyle );
    if ( !(nStyle & WB_NOBORDER) && ( nStyle & WB_DROPDOWN ) )
        nStyle |= WB_BORDER;

    Control::ImplInit( pParent, nStyle, NULL );
    SetBackground();

    ::com::sun::star::uno::Reference< ::com::sun::star::datatransfer::dnd::XDropTargetListener> xDrop = new DNDEventDispatcher(this);

    if( nStyle & WB_DROPDOWN )
    {
        sal_Int32 nLeft, nTop, nRight, nBottom;
        GetBorder( nLeft, nTop, nRight, nBottom );
        mnDDHeight = (sal_uInt16)(GetTextHeight() + nTop + nBottom + 4);

        if( IsNativeWidgetEnabled() &&
            IsNativeControlSupported( CTRL_LISTBOX, PART_ENTIRE_CONTROL ) )
        {
                ImplControlValue aControlValue;
                Rectangle aCtrlRegion( Point( 0, 0 ), Size( 20, mnDDHeight ) );
                Rectangle aBoundingRgn( aCtrlRegion );
                Rectangle aContentRgn( aCtrlRegion );
                if( GetNativeControlRegion( CTRL_LISTBOX, PART_ENTIRE_CONTROL, aCtrlRegion,
                                            CTRL_STATE_ENABLED, aControlValue, OUString(),
                                            aBoundingRgn, aContentRgn ) )
                {
                    sal_Int32 nHeight = aBoundingRgn.GetHeight();
                    if( nHeight > mnDDHeight )
                        mnDDHeight = static_cast<sal_uInt16>(nHeight);
                }
        }

        mpFloatWin = new ImplListBoxFloatingWindow( this );
        mpFloatWin->SetAutoWidth( true );
        mpFloatWin->SetPopupModeEndHdl( LINK( this, ListBox, ImplPopupModeEndHdl ) );
        mpFloatWin->GetDropTarget()->addDropTargetListener(xDrop);

        mpImplWin = new ImplWin( this, (nStyle & (WB_LEFT|WB_RIGHT|WB_CENTER))|WB_NOBORDER );
        mpImplWin->SetMBDownHdl( LINK( this, ListBox, ImplClickBtnHdl ) );
        mpImplWin->SetUserDrawHdl( LINK( this, ListBox, ImplUserDrawHdl ) );
        mpImplWin->Show();
        mpImplWin->GetDropTarget()->addDropTargetListener(xDrop);
        mpImplWin->SetEdgeBlending(GetEdgeBlending());

        mpBtn = new ImplBtn( this, WB_NOLIGHTBORDER | WB_RECTSTYLE );
        ImplInitDropDownButton( mpBtn );
        mpBtn->SetMBDownHdl( LINK( this, ListBox, ImplClickBtnHdl ) );
        mpBtn->Show();
        mpBtn->GetDropTarget()->addDropTargetListener(xDrop);
    }

    Window* pLBParent = this;
    if ( mpFloatWin )
        pLBParent = mpFloatWin;
    mpImplLB = new ImplListBox( pLBParent, nStyle&(~WB_BORDER) );
    mpImplLB->SetSelectHdl( LINK( this, ListBox, ImplSelectHdl ) );
    mpImplLB->SetScrollHdl( LINK( this, ListBox, ImplScrollHdl ) );
    mpImplLB->SetCancelHdl( LINK( this, ListBox, ImplCancelHdl ) );
    mpImplLB->SetDoubleClickHdl( LINK( this, ListBox, ImplDoubleClickHdl ) );
    mpImplLB->SetUserDrawHdl( LINK( this, ListBox, ImplUserDrawHdl ) );
    mpImplLB->SetFocusHdl( LINK( this, ListBox, ImplFocusHdl ) );
    mpImplLB->SetListItemSelectHdl( LINK( this, ListBox, ImplListItemSelectHdl ) );
    mpImplLB->SetPosPixel( Point() );
    mpImplLB->SetEdgeBlending(GetEdgeBlending());
    mpImplLB->Show();

    mpImplLB->GetDropTarget()->addDropTargetListener(xDrop);
    mpImplLB->SetDropTraget(xDrop);

    if ( mpFloatWin )
    {
        mpFloatWin->SetImplListBox( mpImplLB );
        mpImplLB->SetSelectionChangedHdl( LINK( this, ListBox, ImplSelectionChangedHdl ) );
    }
    else
        mpImplLB->GetMainWindow().AllowGrabFocus( true );

    SetCompoundControl( true );
}

WinBits ListBox::ImplInitStyle( WinBits nStyle )
{
    if ( !(nStyle & WB_NOTABSTOP) )
        nStyle |= WB_TABSTOP;
    if ( !(nStyle & WB_NOGROUP) )
        nStyle |= WB_GROUP;
    return nStyle;
}

void ListBox::ImplLoadRes( const ResId& rResId )
{
    Control::ImplLoadRes( rResId );

    // The resource short is actually to be treated as unsigned short.
    sal_uInt16 nResPos = static_cast<sal_uInt16>(ReadShortRes());
    sal_Int32 nSelPos = (nResPos == SAL_MAX_UINT16) ? LISTBOX_ENTRY_NOTFOUND : nResPos;
    sal_Int32 nNumber = ReadLongRes();

    for( sal_Int32 i = 0; i < nNumber; i++ )
    {
        sal_Int32 nPos = InsertEntry( ReadStringRes(), LISTBOX_APPEND );

        sal_IntPtr nId = ReadLongRes();
        if( nId )
            SetEntryData( nPos, (void *)nId ); // ID as UserData
    }

    if( nSelPos < nNumber )
        SelectEntryPos( nSelPos );
}

IMPL_LINK_NOARG(ListBox, ImplSelectHdl)
{
    bool bPopup = IsInDropDown();
    if( IsDropDownBox() )
    {
        if( !mpImplLB->IsTravelSelect() )
        {
            mpFloatWin->EndPopupMode();
            mpImplWin->GrabFocus();
        }

        mpImplWin->SetItemPos( GetSelectEntryPos() );
        mpImplWin->SetString( GetSelectEntry() );
        if( mpImplLB->GetEntryList()->HasImages() )
        {
            Image aImage = mpImplLB->GetEntryList()->GetEntryImage( GetSelectEntryPos() );
            mpImplWin->SetImage( aImage );
        }
        mpImplWin->Invalidate();
    }

    if ( ( !IsTravelSelect() || mpImplLB->IsSelectionChanged() ) || ( bPopup && !IsMultiSelectionEnabled() ) )
        Select();

    return 1;
}

IMPL_LINK( ListBox, ImplFocusHdl, void *, nPos )
{
    ImplCallEventListeners( VCLEVENT_LISTBOX_FOCUS , nPos);
    return 1;
}

IMPL_LINK( ListBox, ImplListItemSelectHdl, void*, EMPTYARG )
{
    ImplCallEventListeners( VCLEVENT_DROPDOWN_SELECT );
    return 1;
}

IMPL_LINK_NOARG(ListBox, ImplScrollHdl)
{
    ImplCallEventListeners( VCLEVENT_LISTBOX_SCROLLED );
    return 1;
}

IMPL_LINK_NOARG(ListBox, ImplCancelHdl)
{
    if( IsInDropDown() )
        mpFloatWin->EndPopupMode();

    return 1;
}

IMPL_LINK( ListBox, ImplSelectionChangedHdl, void*, n )
{
    if ( !mpImplLB->IsTrackingSelect() )
    {
        sal_Int32 nChanged = (sal_Int32)(sal_uLong)n;
        const ImplEntryList* pEntryList = mpImplLB->GetEntryList();
        if ( pEntryList->IsEntryPosSelected( nChanged ) )
        {
            // FIXME? This should've been turned into an ImplPaintEntry some time ago...
            if ( nChanged < pEntryList->GetMRUCount() )
                nChanged = pEntryList->FindEntry( pEntryList->GetEntryText( nChanged ) );
            mpImplWin->SetItemPos( nChanged );
            mpImplWin->SetString( mpImplLB->GetEntryList()->GetEntryText( nChanged ) );
            if( mpImplLB->GetEntryList()->HasImages() )
            {
                Image aImage = mpImplLB->GetEntryList()->GetEntryImage( nChanged );
                mpImplWin->SetImage( aImage );
            }
        }
        else
        {
            mpImplWin->SetItemPos( LISTBOX_ENTRY_NOTFOUND );
            mpImplWin->SetString( OUString() );
            Image aImage;
            mpImplWin->SetImage( aImage );
        }
        mpImplWin->Invalidate();
    }
    return 1;
}

IMPL_LINK_NOARG(ListBox, ImplDoubleClickHdl)
{
    DoubleClick();
    return 1;
}

IMPL_LINK_NOARG(ListBox, ImplClickBtnHdl)
{
    if( !mpFloatWin->IsInPopupMode() )
    {
        ImplCallEventListeners( VCLEVENT_DROPDOWN_PRE_OPEN );
        mpImplWin->GrabFocus();
        mpBtn->SetPressed( true );
        mpFloatWin->StartFloat( true );
        ImplCallEventListeners( VCLEVENT_DROPDOWN_OPEN );

        ImplClearLayoutData();
        if( mpImplLB )
            mpImplLB->GetMainWindow().ImplClearLayoutData();
        if( mpImplWin )
            mpImplWin->ImplClearLayoutData();
    }

    return 0;
}

IMPL_LINK_NOARG(ListBox, ImplPopupModeEndHdl)
{
    if( mpFloatWin->IsPopupModeCanceled() )
    {
        if ( ( mpFloatWin->GetPopupModeStartSaveSelection() != LISTBOX_ENTRY_NOTFOUND )
                && !IsEntryPosSelected( mpFloatWin->GetPopupModeStartSaveSelection() ) )
        {
            mpImplLB->SelectEntry( mpFloatWin->GetPopupModeStartSaveSelection(), true );
            bool bTravelSelect = mpImplLB->IsTravelSelect();
            mpImplLB->SetTravelSelect( true );

            ImplDelData aCheckDelete;
            ImplAddDel( &aCheckDelete );
            Select();
            if ( aCheckDelete.IsDead() )
                return 0;
            ImplRemoveDel( &aCheckDelete );

            mpImplLB->SetTravelSelect( bTravelSelect );
        }
    }

    ImplClearLayoutData();
    if( mpImplLB )
        mpImplLB->GetMainWindow().ImplClearLayoutData();
    if( mpImplWin )
        mpImplWin->ImplClearLayoutData();

    mpBtn->SetPressed( false );
    ImplCallEventListeners( VCLEVENT_DROPDOWN_CLOSE );
    return 0;
}

void ListBox::ToggleDropDown()
{
    if( IsDropDownBox() )
    {
        if( mpFloatWin->IsInPopupMode() )
            mpFloatWin->EndPopupMode();
        else
        {
            ImplCallEventListeners( VCLEVENT_DROPDOWN_PRE_OPEN );
            mpImplWin->GrabFocus();
            mpBtn->SetPressed( true );
            mpFloatWin->StartFloat( true );
            ImplCallEventListeners( VCLEVENT_DROPDOWN_OPEN );
        }
    }
}

void ListBox::Draw( OutputDevice* pDev, const Point& rPos, const Size& rSize, sal_uLong nFlags )
{
    mpImplLB->GetMainWindow().ImplInitSettings( true, true, true );

    Point aPos = pDev->LogicToPixel( rPos );
    Size aSize = pDev->LogicToPixel( rSize );
    Font aFont = mpImplLB->GetMainWindow().GetDrawPixelFont( pDev );
    OutDevType eOutDevType = pDev->GetOutDevType();

    pDev->Push();
    pDev->SetMapMode();
    pDev->SetFont( aFont );
    pDev->SetTextFillColor();

    // Border/Background
    pDev->SetLineColor();
    pDev->SetFillColor();
    bool bBorder = !(nFlags & WINDOW_DRAW_NOBORDER ) && (GetStyle() & WB_BORDER);
    bool bBackground = !(nFlags & WINDOW_DRAW_NOBACKGROUND) && IsControlBackground();
    if ( bBorder || bBackground )
    {
        Rectangle aRect( aPos, aSize );
        if ( bBorder )
        {
            ImplDrawFrame( pDev, aRect );
        }
        if ( bBackground )
        {
            pDev->SetFillColor( GetControlBackground() );
            pDev->DrawRect( aRect );
        }
    }

    // Content
    if ( ( nFlags & WINDOW_DRAW_MONO ) || ( eOutDevType == OUTDEV_PRINTER ) )
    {
        pDev->SetTextColor( Color( COL_BLACK ) );
    }
    else
    {
        if ( !(nFlags & WINDOW_DRAW_NODISABLE ) && !IsEnabled() )
        {
            const StyleSettings& rStyleSettings = GetSettings().GetStyleSettings();
            pDev->SetTextColor( rStyleSettings.GetDisableColor() );
        }
        else
        {
            pDev->SetTextColor( GetTextColor() );
        }
    }

    long nOnePixel = GetDrawPixel( pDev, 1 );
    sal_uInt16 nTextStyle = TEXT_DRAW_VCENTER;
    Rectangle aTextRect( aPos, aSize );

    if ( GetStyle() & WB_CENTER )
        nTextStyle |= TEXT_DRAW_CENTER;
    else if ( GetStyle() & WB_RIGHT )
        nTextStyle |= TEXT_DRAW_RIGHT;
    else
        nTextStyle |= TEXT_DRAW_LEFT;

    aTextRect.Left() += 3*nOnePixel;
    aTextRect.Right() -= 3*nOnePixel;

    if ( IsDropDownBox() )
    {
        OUString   aText = GetSelectEntry();
        long       nTextHeight = pDev->GetTextHeight();
        long       nTextWidth = pDev->GetTextWidth( aText );
        long       nOffX = 3*nOnePixel;
        long       nOffY = (aSize.Height()-nTextHeight) / 2;

        // Clipping?
        if ( (nOffY < 0) ||
             ((nOffY+nTextHeight) > aSize.Height()) ||
             ((nOffX+nTextWidth) > aSize.Width()) )
        {
            Rectangle aClip( aPos, aSize );
            if ( nTextHeight > aSize.Height() )
                aClip.Bottom() += nTextHeight-aSize.Height()+1;  // So that HP Printers don't optimize this away
            pDev->IntersectClipRegion( aClip );
        }

        pDev->DrawText( aTextRect, aText, nTextStyle );
    }
    else
    {
        long        nTextHeight = pDev->GetTextHeight();
        sal_uInt16  nLines = (sal_uInt16)(aSize.Height() / nTextHeight);
        Rectangle   aClip( aPos, aSize );

        pDev->IntersectClipRegion( aClip );

        if ( !nLines )
            nLines = 1;

        for ( sal_uInt16 n = 0; n < nLines; n++ )
        {
            sal_Int32 nEntry = n+mpImplLB->GetTopEntry();
            bool bSelected = mpImplLB->GetEntryList()->IsEntryPosSelected( nEntry );
            if ( bSelected )
            {
                pDev->SetFillColor( COL_BLACK );
                pDev->DrawRect( Rectangle(  Point( aPos.X(), aPos.Y() + n*nTextHeight ),
                                            Point( aPos.X() + aSize.Width(), aPos.Y() + (n+1)*nTextHeight + 2*nOnePixel ) ) );
                pDev->SetFillColor();
                pDev->SetTextColor( COL_WHITE );
            }

            aTextRect.Top() = aPos.Y() + n*nTextHeight;
            aTextRect.Bottom() = aTextRect.Top() + nTextHeight;

            pDev->DrawText( aTextRect, mpImplLB->GetEntryList()->GetEntryText( nEntry ), nTextStyle );

            if ( bSelected )
                pDev->SetTextColor( COL_BLACK );
        }
    }

    pDev->Pop();
}

void ListBox::GetFocus()
{
    if ( mpImplLB )
    {
        if( IsDropDownBox() )
            mpImplWin->GrabFocus();
        else
            mpImplLB->GrabFocus();
    }

    Control::GetFocus();
}

Window* ListBox::GetPreferredKeyInputWindow()
{
    if ( mpImplLB )
    {
        if( IsDropDownBox() )
            return mpImplWin->GetPreferredKeyInputWindow();
        else
            return mpImplLB->GetPreferredKeyInputWindow();
    }

    return Control::GetPreferredKeyInputWindow();
}

void ListBox::LoseFocus()
{
    if( IsDropDownBox() )
        mpImplWin->HideFocus();
    else
        mpImplLB->HideFocus();

    Control::LoseFocus();
}

void ListBox::DataChanged( const DataChangedEvent& rDCEvt )
{
    Control::DataChanged( rDCEvt );

    if ( (rDCEvt.GetType() == DATACHANGED_FONTS) ||
         (rDCEvt.GetType() == DATACHANGED_FONTSUBSTITUTION) ||
         ((rDCEvt.GetType() == DATACHANGED_SETTINGS) &&
          (rDCEvt.GetFlags() & SETTINGS_STYLE)) )
    {
        SetBackground();    // Due to a hack in Window::UpdateSettings the background must be reset
                            // otherwise it will overpaint NWF drawn listboxes
        Resize();
        mpImplLB->Resize(); // Is not called by ListBox::Resize() if the ImplLB does not change

        if ( mpImplWin )
        {
            mpImplWin->SetSettings( GetSettings() ); // If not yet set...
            ImplInitFieldSettings( mpImplWin, true, true, true );

            mpBtn->SetSettings( GetSettings() );
            ImplInitDropDownButton( mpBtn );
        }

        if ( IsDropDownBox() )
            Invalidate();
    }
}

void ListBox::EnableAutoSize( bool bAuto )
{
    mbDDAutoSize = bAuto;
    if ( mpFloatWin )
    {
        if ( bAuto && !mpFloatWin->GetDropDownLineCount() )
        {
            // use GetListBoxMaximumLineCount here; before, was on fixed number of five
            AdaptDropDownLineCountToMaximum();
        }
        else if ( !bAuto )
        {
            mpFloatWin->SetDropDownLineCount( 0 );
        }
    }
}

void ListBox::EnableDDAutoWidth( bool b )
{
    if ( mpFloatWin )
        mpFloatWin->SetAutoWidth( b );
}

void ListBox::SetDropDownLineCount( sal_uInt16 nLines )
{
    mnLineCount = nLines;
    if ( mpFloatWin )
        mpFloatWin->SetDropDownLineCount( mnLineCount );
}

void ListBox::AdaptDropDownLineCountToMaximum()
{
    // adapt to maximum allowed number
    SetDropDownLineCount(GetSettings().GetStyleSettings().GetListBoxMaximumLineCount());
}

sal_uInt16 ListBox::GetDropDownLineCount() const
{
    if ( mpFloatWin )
        return mpFloatWin->GetDropDownLineCount();
    return mnLineCount;
}

void ListBox::setPosSizePixel( long nX, long nY, long nWidth, long nHeight, sal_uInt16 nFlags )
{
    if( IsDropDownBox() && ( nFlags & WINDOW_POSSIZE_SIZE ) )
    {
        Size aPrefSz = mpFloatWin->GetPrefSize();
        if ( ( nFlags & WINDOW_POSSIZE_HEIGHT ) && ( nHeight >= 2*mnDDHeight ) )
            aPrefSz.Height() = nHeight-mnDDHeight;
        if ( nFlags & WINDOW_POSSIZE_WIDTH )
            aPrefSz.Width() = nWidth;
        mpFloatWin->SetPrefSize( aPrefSz );

        if ( IsAutoSizeEnabled() && ! (nFlags & WINDOW_POSSIZE_DROPDOWN) )
            nHeight = mnDDHeight;
    }

    Control::setPosSizePixel( nX, nY, nWidth, nHeight, nFlags );
}

void ListBox::Resize()
{
    Size aOutSz = GetOutputSizePixel();
    if( IsDropDownBox() )
    {
        // Initialize the dropdown button size with the standard scrollbar width
        long nSBWidth = GetSettings().GetStyleSettings().GetScrollBarSize();
        long nBottom = aOutSz.Height();

        // Note: in case of no border, pBorder will actually be this
        Window *pBorder = GetWindow( WINDOW_BORDER );
        ImplControlValue aControlValue;
        Point aPoint;
        Rectangle aContent, aBound;

        // Use the full extent of the control
        Rectangle aArea( aPoint, pBorder->GetOutputSizePixel() );

        if ( GetNativeControlRegion( CTRL_LISTBOX, PART_BUTTON_DOWN,
                    aArea, 0, aControlValue, OUString(), aBound, aContent) )
        {
            long nTop = 0;
            // Convert back from border space to local coordinates
            aPoint = pBorder->ScreenToOutputPixel( OutputToScreenPixel( aPoint ) );
            aContent.Move( -aPoint.X(), -aPoint.Y() );

            // Use the themes drop down size for the button
            aOutSz.Width() = aContent.Left();
            mpBtn->setPosSizePixel( aContent.Left(), nTop, aContent.Right(), (nBottom-nTop) );

            // Adjust the size of the edit field
            if ( GetNativeControlRegion( CTRL_LISTBOX, PART_SUB_EDIT,
                        aArea, 0, aControlValue, OUString(), aBound, aContent) )
            {
                // Convert back from border space to local coordinates
                aContent.Move( -aPoint.X(), -aPoint.Y() );

                // Use the themes drop down size
                if( ! (GetStyle() & WB_BORDER) && ImplGetSVData()->maNWFData.mbNoFocusRects )
                {
                    // No border but focus ring behavior -> we have a problem; the
                    // native rect relies on the border to draw the focus
                    // let's do the best we can and center vertically, so it doesn't look
                    // completely wrong.
                    Size aSz( GetOutputSizePixel() );
                    long nDiff = aContent.Top() - (aSz.Height() - aContent.GetHeight())/2;
                    aContent.Top() -= nDiff;
                    aContent.Bottom() -= nDiff;
                }
                mpImplWin->SetPosSizePixel( aContent.TopLeft(), aContent.GetSize() );
            }
            else
                mpImplWin->SetSizePixel( aOutSz );
        }
        else
        {
            nSBWidth = CalcZoom( nSBWidth );
            mpImplWin->setPosSizePixel( 0, 0, aOutSz.Width() - nSBWidth, aOutSz.Height() );
            mpBtn->setPosSizePixel( aOutSz.Width() - nSBWidth, 0, nSBWidth, aOutSz.Height() );
        }
    }
    else
    {
        mpImplLB->SetSizePixel( aOutSz );
    }

    // Retain FloatingWindow size even when it's invisible, as we still process KEY_PGUP/DOWN ...
    if ( mpFloatWin )
        mpFloatWin->SetSizePixel( mpFloatWin->CalcFloatSize() );

    Control::Resize();
}

void ListBox::FillLayoutData() const
{
    mpControlData->mpLayoutData = new vcl::ControlLayoutData();
    const Control& rMainWin = mpImplLB->GetMainWindow();
    if( mpFloatWin )
    {
        // Dropdown mode
        AppendLayoutData( *mpImplWin );
        mpImplWin->SetLayoutDataParent( this );
        if( mpFloatWin->IsReallyVisible() )
        {
            AppendLayoutData( rMainWin );
            rMainWin.SetLayoutDataParent( this );
        }
    }
    else
    {
        AppendLayoutData( rMainWin );
        rMainWin.SetLayoutDataParent( this );
    }
}

long ListBox::GetIndexForPoint( const Point& rPoint, sal_Int32& rPos ) const
{
    if( !HasLayoutData() )
        FillLayoutData();

    // Check whether rPoint fits at all
    long nIndex = Control::GetIndexForPoint( rPoint );
    if( nIndex != -1 )
    {
        // Point must be either in main list window
        // or in impl window (dropdown case)
        ImplListBoxWindow& rMain = mpImplLB->GetMainWindow();

        // Convert coordinates to ImplListBoxWindow pixel coordinate space
        Point aConvPoint = LogicToPixel( rPoint );
        aConvPoint = OutputToAbsoluteScreenPixel( aConvPoint );
        aConvPoint = rMain.AbsoluteScreenToOutputPixel( aConvPoint );
        aConvPoint = rMain.PixelToLogic( aConvPoint );

        // Try to find entry
        sal_Int32 nEntry = rMain.GetEntryPosForPoint( aConvPoint );
        if( nEntry == LISTBOX_ENTRY_NOTFOUND )
        {
            // Not found, maybe dropdown case
            if( mpImplWin && mpImplWin->IsReallyVisible() )
            {
                // Convert to impl window pixel coordinates
                aConvPoint = LogicToPixel( rPoint );
                aConvPoint = OutputToAbsoluteScreenPixel( aConvPoint );
                aConvPoint = mpImplWin->AbsoluteScreenToOutputPixel( aConvPoint );

                // Check whether converted point is inside impl window
                Size aImplWinSize = mpImplWin->GetOutputSizePixel();
                if( aConvPoint.X() >= 0 && aConvPoint.Y() >= 0 && aConvPoint.X() < aImplWinSize.Width() && aConvPoint.Y() < aImplWinSize.Height() )
                {
                    // Inside the impl window, the position is the current item pos
                    rPos = mpImplWin->GetItemPos();
                }
                else
                    nIndex = -1;
            }
            else
                nIndex = -1;
        }
        else
            rPos = nEntry;

        DBG_ASSERT( nIndex != -1, "found index for point, but relative index failed" );
    }

    // Get line relative index
    if( nIndex != -1 )
        nIndex = ToRelativeLineIndex( nIndex );

    return nIndex;
}

void ListBox::StateChanged( StateChangedType nType )
{
    if( nType == STATE_CHANGE_READONLY )
    {
        if( mpImplWin )
            mpImplWin->Enable( !IsReadOnly() );
        if( mpBtn )
            mpBtn->Enable( !IsReadOnly() );
    }
    else if( nType == STATE_CHANGE_ENABLE )
    {
        mpImplLB->Enable( IsEnabled() );
        if( mpImplWin )
        {
            mpImplWin->Enable( IsEnabled() );
            if ( IsNativeControlSupported(CTRL_LISTBOX, PART_ENTIRE_CONTROL)
                    && ! IsNativeControlSupported(CTRL_LISTBOX, PART_BUTTON_DOWN) )
            {
                GetWindow( WINDOW_BORDER )->Invalidate( INVALIDATE_NOERASE );
            }
            else
                mpImplWin->Invalidate();
        }
        if( mpBtn )
            mpBtn->Enable( IsEnabled() );
    }
    else if( nType == STATE_CHANGE_UPDATEMODE )
    {
        mpImplLB->SetUpdateMode( IsUpdateMode() );
    }
    else if ( nType == STATE_CHANGE_ZOOM )
    {
        mpImplLB->SetZoom( GetZoom() );
        if ( mpImplWin )
        {
            mpImplWin->SetZoom( GetZoom() );
            mpImplWin->SetFont( mpImplLB->GetMainWindow().GetFont() );
            mpImplWin->Invalidate();
        }
        Resize();
    }
    else if ( nType == STATE_CHANGE_CONTROLFONT )
    {
        mpImplLB->SetControlFont( GetControlFont() );
        if ( mpImplWin )
        {
            mpImplWin->SetControlFont( GetControlFont() );
            mpImplWin->SetFont( mpImplLB->GetMainWindow().GetFont() );
            mpImplWin->Invalidate();
        }
        Resize();
    }
    else if ( nType == STATE_CHANGE_CONTROLFOREGROUND )
    {
        mpImplLB->SetControlForeground( GetControlForeground() );
        if ( mpImplWin )
        {
            mpImplWin->SetControlForeground( GetControlForeground() );
            mpImplWin->SetTextColor( GetControlForeground() );
            mpImplWin->SetFont( mpImplLB->GetMainWindow().GetFont() );
            mpImplWin->Invalidate();
        }
    }
    else if ( nType == STATE_CHANGE_CONTROLBACKGROUND )
    {
        mpImplLB->SetControlBackground( GetControlBackground() );
        if ( mpImplWin )
        {
            if ( mpImplWin->IsNativeControlSupported(CTRL_LISTBOX, PART_ENTIRE_CONTROL) )
            {
                // Transparent background
                mpImplWin->SetBackground();
                mpImplWin->SetControlBackground();
            }
            else
            {
                mpImplWin->SetBackground( mpImplLB->GetMainWindow().GetControlBackground() );
                mpImplWin->SetControlBackground( mpImplLB->GetMainWindow().GetControlBackground() );
            }
            mpImplWin->SetFont( mpImplLB->GetMainWindow().GetFont() );
            mpImplWin->Invalidate();
        }
    }
    else if ( nType == STATE_CHANGE_STYLE )
    {
        SetStyle( ImplInitStyle( GetStyle() ) );
        mpImplLB->GetMainWindow().EnableSort( ( GetStyle() & WB_SORT ) ? true : false );
        bool bSimpleMode = ( GetStyle() & WB_SIMPLEMODE ) ? true : false;
        mpImplLB->SetMultiSelectionSimpleMode( bSimpleMode );
    }
    else if( nType == STATE_CHANGE_MIRRORING )
    {
        if( mpBtn )
        {
            mpBtn->EnableRTL( IsRTLEnabled() );
            ImplInitDropDownButton( mpBtn );
        }
        mpImplLB->EnableRTL( IsRTLEnabled() );
        if( mpImplWin )
            mpImplWin->EnableRTL( IsRTLEnabled() );
        Resize();
    }

    Control::StateChanged( nType );
}

bool ListBox::PreNotify( NotifyEvent& rNEvt )
{
    bool nDone = false;
    if ( mpImplLB )
    {
        if( ( rNEvt.GetType() == EVENT_KEYINPUT ) && ( rNEvt.GetWindow() == mpImplWin ) )
        {
            KeyEvent aKeyEvt = *rNEvt.GetKeyEvent();
            switch( aKeyEvt.GetKeyCode().GetCode() )
            {
                case KEY_DOWN:
                {
                    if( mpFloatWin && !mpFloatWin->IsInPopupMode() &&
                        aKeyEvt.GetKeyCode().IsMod2() )
                    {
                        ImplCallEventListeners( VCLEVENT_DROPDOWN_PRE_OPEN );
                        mpBtn->SetPressed( true );
                        mpFloatWin->StartFloat( false );
                        ImplCallEventListeners( VCLEVENT_DROPDOWN_OPEN );
                        nDone = true;
                    }
                    else
                    {
                        nDone = mpImplLB->ProcessKeyInput( aKeyEvt );
                    }
                }
                break;
                case KEY_UP:
                {
                    if( mpFloatWin && mpFloatWin->IsInPopupMode() &&
                        aKeyEvt.GetKeyCode().IsMod2() )
                    {
                        mpFloatWin->EndPopupMode();
                        nDone = true;
                    }
                    else
                    {
                        nDone = mpImplLB->ProcessKeyInput( aKeyEvt );
                    }
                }
                break;
                case KEY_RETURN:
                {
                    if( IsInDropDown() )
                    {
                        mpImplLB->ProcessKeyInput( aKeyEvt );
                        nDone = true;
                    }
                }
                break;

                default:
                {
                    nDone = mpImplLB->ProcessKeyInput( aKeyEvt );
                }
            }
        }
        else if ( rNEvt.GetType() == EVENT_LOSEFOCUS )
        {
            if ( IsInDropDown() && !HasChildPathFocus( true ) )
                mpFloatWin->EndPopupMode();
        }
        else if ( (rNEvt.GetType() == EVENT_COMMAND) &&
                  (rNEvt.GetCommandEvent()->GetCommand() == COMMAND_WHEEL) &&
                  (rNEvt.GetWindow() == mpImplWin) )
        {
            sal_uInt16 nWheelBehavior( GetSettings().GetMouseSettings().GetWheelBehavior() );
            if  (   ( nWheelBehavior == MOUSE_WHEEL_ALWAYS )
                ||  (   ( nWheelBehavior == MOUSE_WHEEL_FOCUS_ONLY )
                    &&  HasChildPathFocus()
                    )
                )
            {
                nDone = mpImplLB->HandleWheelAsCursorTravel( *rNEvt.GetCommandEvent() );
            }
            else
            {
                nDone = false;  // Don't consume this event, let the default handling take it (i.e. scroll the context)
            }
        }
    }

    return nDone || Control::PreNotify( rNEvt );
}

void ListBox::Select()
{
    ImplCallEventListenersAndHandler( VCLEVENT_LISTBOX_SELECT, maSelectHdl, this );
}

void ListBox::DoubleClick()
{
    ImplCallEventListenersAndHandler( VCLEVENT_LISTBOX_DOUBLECLICK, maDoubleClickHdl, this );
}

void ListBox::Clear()
{
    mpImplLB->Clear();
    if( IsDropDownBox() )
    {
        mpImplWin->SetItemPos( LISTBOX_ENTRY_NOTFOUND );
        mpImplWin->SetString( OUString() );
        Image aImage;
        mpImplWin->SetImage( aImage );
        mpImplWin->Invalidate();
    }
    CallEventListeners( VCLEVENT_LISTBOX_ITEMREMOVED, (void*) sal_IntPtr(-1) );
}

void ListBox::SetNoSelection()
{
    mpImplLB->SetNoSelection();
    if( IsDropDownBox() )
    {
        mpImplWin->SetItemPos( LISTBOX_ENTRY_NOTFOUND );
        mpImplWin->SetString( OUString() );
        Image aImage;
        mpImplWin->SetImage( aImage );
        mpImplWin->Invalidate();
    }
    ImplCallEventListeners(VCLEVENT_LISTBOX_STATEUPDATE);
}

sal_Int32 ListBox::InsertEntry( const OUString& rStr, sal_Int32 nPos )
{
    sal_Int32 nRealPos = mpImplLB->InsertEntry( nPos + mpImplLB->GetEntryList()->GetMRUCount(), rStr );
    nRealPos = sal::static_int_cast<sal_Int32>(nRealPos - mpImplLB->GetEntryList()->GetMRUCount());
    CallEventListeners( VCLEVENT_LISTBOX_ITEMADDED, (void*) sal_IntPtr(nRealPos) );
    return nRealPos;
}

sal_Int32 ListBox::InsertEntry( const OUString& rStr, const Image& rImage, sal_Int32 nPos )
{
    sal_Int32 nRealPos = mpImplLB->InsertEntry( nPos + mpImplLB->GetEntryList()->GetMRUCount(), rStr, rImage );
    nRealPos = sal::static_int_cast<sal_Int32>(nRealPos - mpImplLB->GetEntryList()->GetMRUCount());
    CallEventListeners( VCLEVENT_LISTBOX_ITEMADDED, (void*) sal_IntPtr(nRealPos) );
    return nRealPos;
}

void ListBox::RemoveEntry( const OUString& rStr )
{
    RemoveEntry( GetEntryPos( rStr ) );
}

void ListBox::RemoveEntry( sal_Int32 nPos )
{
    mpImplLB->RemoveEntry( nPos + mpImplLB->GetEntryList()->GetMRUCount() );
    CallEventListeners( VCLEVENT_LISTBOX_ITEMREMOVED, (void*) sal_IntPtr(nPos) );
}

Image ListBox::GetEntryImage( sal_Int32 nPos ) const
{
    if ( mpImplLB->GetEntryList()->HasEntryImage( nPos ) )
        return mpImplLB->GetEntryList()->GetEntryImage( nPos );
    return Image();
}

sal_Int32 ListBox::GetEntryPos( const OUString& rStr ) const
{
    sal_Int32 nPos = mpImplLB->GetEntryList()->FindEntry( rStr );
    if ( nPos != LISTBOX_ENTRY_NOTFOUND )
        nPos = nPos - mpImplLB->GetEntryList()->GetMRUCount();
    return nPos;
}

sal_Int32 ListBox::GetEntryPos( const void* pData ) const
{
    sal_Int32 nPos = mpImplLB->GetEntryList()->FindEntry( pData );
    if ( nPos != LISTBOX_ENTRY_NOTFOUND )
        nPos = nPos - mpImplLB->GetEntryList()->GetMRUCount();
    return nPos;
}

OUString ListBox::GetEntry( sal_Int32 nPos ) const
{
    return mpImplLB->GetEntryList()->GetEntryText( nPos + mpImplLB->GetEntryList()->GetMRUCount() );
}

sal_Int32 ListBox::GetEntryCount() const
{
    return mpImplLB->GetEntryList()->GetEntryCount() - mpImplLB->GetEntryList()->GetMRUCount();
}

OUString ListBox::GetSelectEntry(sal_Int32 nIndex) const
{
    return GetEntry( GetSelectEntryPos( nIndex ) );
}

sal_Int32 ListBox::GetSelectEntryCount() const
{
    return mpImplLB->GetEntryList()->GetSelectEntryCount();
}

sal_Int32 ListBox::GetSelectEntryPos( sal_Int32 nIndex ) const
{
    sal_Int32 nPos = mpImplLB->GetEntryList()->GetSelectEntryPos( nIndex );
    if ( nPos != LISTBOX_ENTRY_NOTFOUND )
    {
        if ( nPos < mpImplLB->GetEntryList()->GetMRUCount() )
            nPos = mpImplLB->GetEntryList()->FindEntry( mpImplLB->GetEntryList()->GetEntryText( nPos ) );
        nPos = nPos - mpImplLB->GetEntryList()->GetMRUCount();
    }
    return nPos;
}

bool ListBox::IsEntrySelected(const OUString& rStr) const
{
    return IsEntryPosSelected( GetEntryPos( rStr ) );
}

bool ListBox::IsEntryPosSelected( sal_Int32 nPos ) const
{
    return mpImplLB->GetEntryList()->IsEntryPosSelected( nPos + mpImplLB->GetEntryList()->GetMRUCount() );
}

void ListBox::SelectEntry( const OUString& rStr, bool bSelect )
{
    SelectEntryPos( GetEntryPos( rStr ), bSelect );
}

void ListBox::SelectEntryPos( sal_Int32 nPos, bool bSelect )
{
    if ( 0 <= nPos && nPos < mpImplLB->GetEntryList()->GetEntryCount() )
    {
        sal_Int32 oldSelectCount = GetSelectEntryCount(), newSelectCount = 0, nCurrentPos = mpImplLB->GetCurrentPos();
        mpImplLB->SelectEntry( nPos + mpImplLB->GetEntryList()->GetMRUCount(), bSelect );
        newSelectCount = GetSelectEntryCount();
        if (oldSelectCount == 0 && newSelectCount > 0)
            ImplCallEventListeners(VCLEVENT_LISTBOX_STATEUPDATE);
        //Only when bSelect == true, send both Selection & Focus events
        if (nCurrentPos != nPos && bSelect)
        {
            ImplCallEventListeners( VCLEVENT_LISTBOX_SELECT, reinterpret_cast<void*>(nPos));
            if (HasFocus())
                ImplCallEventListeners( VCLEVENT_LISTBOX_FOCUS, reinterpret_cast<void*>(nPos));
        }
    }
}

void ListBox::SetEntryData( sal_Int32 nPos, void* pNewData )
{
    mpImplLB->SetEntryData( nPos + mpImplLB->GetEntryList()->GetMRUCount(), pNewData );
}

void* ListBox::GetEntryData( sal_Int32 nPos ) const
{
    return mpImplLB->GetEntryList()->GetEntryData( nPos + mpImplLB->GetEntryList()->GetMRUCount() );
}

void ListBox::SetEntryFlags( sal_Int32 nPos, long nFlags )
{
    mpImplLB->SetEntryFlags( nPos + mpImplLB->GetEntryList()->GetMRUCount(), nFlags );
}

long ListBox::GetEntryFlags( sal_Int32 nPos ) const
{
    return mpImplLB->GetEntryList()->GetEntryFlags( nPos + mpImplLB->GetEntryList()->GetMRUCount() );
}

void ListBox::SetTopEntry( sal_Int32 nPos )
{
    mpImplLB->SetTopEntry( nPos + mpImplLB->GetEntryList()->GetMRUCount() );
}

sal_Int32 ListBox::GetTopEntry() const
{
    sal_Int32 nPos = GetEntryCount() ? mpImplLB->GetTopEntry() : LISTBOX_ENTRY_NOTFOUND;
    if ( nPos < mpImplLB->GetEntryList()->GetMRUCount() )
        nPos = 0;
    return nPos;
}

bool ListBox::IsTravelSelect() const
{
    return mpImplLB->IsTravelSelect();
}

bool ListBox::IsInDropDown() const
{
    return mpFloatWin && mpFloatWin->IsInPopupMode();
}

Rectangle ListBox::GetBoundingRectangle( sal_Int32 nItem ) const
{
    Rectangle aRect = mpImplLB->GetMainWindow().GetBoundingRectangle( nItem );
    Rectangle aOffset = mpImplLB->GetMainWindow().GetWindowExtentsRelative( (Window*)this );
    aRect.Move( aOffset.TopLeft().X(), aOffset.TopLeft().Y() );
    return aRect;
}

void ListBox::EnableMultiSelection( bool bMulti )
{
    EnableMultiSelection( bMulti, false );
}

void ListBox::EnableMultiSelection( bool bMulti, bool bStackSelection )
{
    mpImplLB->EnableMultiSelection( bMulti, bStackSelection );

    // WB_SIMPLEMODE:
    // The MultiListBox behaves just like a normal ListBox
    // MultiSelection is possible via corresponding additional keys
    bool bSimpleMode = ( GetStyle() & WB_SIMPLEMODE ) ? true : false;
    mpImplLB->SetMultiSelectionSimpleMode( bSimpleMode );

    // In a MultiSelection, we can't see us travelling without focus
    if ( mpFloatWin )
        mpImplLB->GetMainWindow().AllowGrabFocus( bMulti );
}

bool ListBox::IsMultiSelectionEnabled() const
{
    return mpImplLB->IsMultiSelectionEnabled();
}

Size ListBox::CalcMinimumSize() const
{
    Size aSz;

    if (!mpImplLB)
        return aSz;

    aSz = CalcSubEditSize();

    bool bAddScrollWidth = false;

    if (IsDropDownBox())
    {
        aSz.Height() += 4; // add a space between entry and border
        aSz.Width() += 4;  // add a little breathing space
        bAddScrollWidth = true;
    }
    else
        bAddScrollWidth = (GetStyle() & WB_VSCROLL) == WB_VSCROLL;

    if (bAddScrollWidth)
    {
        // Try native borders; scrollbar size may not be a good indicator
        // See how large the edit area inside is to estimate what is needed for the dropdown
        ImplControlValue aControlValue;
        Point aPoint;
        Rectangle aContent, aBound;
        Size aTestSize( 100, 20 );
        Rectangle aArea( aPoint, aTestSize );
        if( GetNativeControlRegion( CTRL_LISTBOX, PART_SUB_EDIT, aArea, 0,
                    aControlValue, OUString(), aBound, aContent) )
        {
            // use the themes drop down size
            aSz.Width() += aTestSize.Width() - aContent.GetWidth();
        }
        else
            aSz.Width() += GetSettings().GetStyleSettings().GetScrollBarSize();
    }

    aSz = CalcWindowSize( aSz );

    if (IsDropDownBox()) // Check minimum height of dropdown box
    {
        ImplControlValue aControlValue;
        Rectangle aRect( Point( 0, 0 ), aSz );
        Rectangle aContent, aBound;
        if( GetNativeControlRegion( CTRL_LISTBOX, PART_ENTIRE_CONTROL, aRect, 0,
                    aControlValue, OUString(), aBound, aContent) )
        {
            if( aBound.GetHeight() > aSz.Height() )
                aSz.Height() = aBound.GetHeight();
        }
    }

    return aSz;
}

Size ListBox::CalcSubEditSize() const
{
    Size aSz;

    if (!mpImplLB)
        return aSz;

    if ( !IsDropDownBox() )
        aSz = mpImplLB->CalcSize (mnLineCount ? mnLineCount : mpImplLB->GetEntryList()->GetEntryCount());
    else
    {
        aSz.Height() = mpImplLB->CalcSize( 1 ).Height();
        // Size to maxmimum entry width
        aSz.Width() = mpImplLB->GetMaxEntryWidth();

        if (m_nMaxWidthChars != -1)
        {
            long nMaxWidth = m_nMaxWidthChars * approximate_char_width();
            aSz.Width() = std::min(aSz.Width(), nMaxWidth);
        }

        // Do not create ultrathin ListBoxes, it doesn't look good
        if( aSz.Width() < GetSettings().GetStyleSettings().GetScrollBarSize() )
            aSz.Width() = GetSettings().GetStyleSettings().GetScrollBarSize();
    }

    return aSz;
}

Size ListBox::GetOptimalSize() const
{
    return CalcMinimumSize();
}

Size ListBox::CalcAdjustedSize( const Size& rPrefSize ) const
{
    Size aSz = rPrefSize;
    sal_Int32 nLeft, nTop, nRight, nBottom;
    ((Window*)this)->GetBorder( nLeft, nTop, nRight, nBottom );
    aSz.Height() -= nTop+nBottom;
    if ( !IsDropDownBox() )
    {
        long nEntryHeight = CalcBlockSize( 1, 1 ).Height();
        long nLines = aSz.Height() / nEntryHeight;
        if ( nLines < 1 )
            nLines = 1;
        aSz.Height() = nLines * nEntryHeight;
    }
    else
    {
        aSz.Height() = mnDDHeight;
    }
    aSz.Height() += nTop+nBottom;

    aSz = CalcWindowSize( aSz );
    return aSz;
}

Size ListBox::CalcBlockSize( sal_uInt16 nColumns, sal_uInt16 nLines ) const
{
    // ScrollBars are shown if needed
    Size aMinSz = CalcMinimumSize();
    // aMinSz = ImplCalcOutSz( aMinSz );

    Size aSz;

    // Height
    if ( nLines )
    {
        if ( !IsDropDownBox() )
            aSz.Height() = mpImplLB->CalcSize( nLines ).Height();
        else
            aSz.Height() = mnDDHeight;
    }
    else
        aSz.Height() = aMinSz.Height();

    // Width
    if ( nColumns )
        aSz.Width() = nColumns * GetTextWidth( OUString('X') );
    else
        aSz.Width() = aMinSz.Width();

    if ( IsDropDownBox() )
        aSz.Width() += GetSettings().GetStyleSettings().GetScrollBarSize();

    if ( !IsDropDownBox() )
    {
        if ( aSz.Width() < aMinSz.Width() )
            aSz.Height() += GetSettings().GetStyleSettings().GetScrollBarSize();
        if ( aSz.Height() < aMinSz.Height() )
            aSz.Width() += GetSettings().GetStyleSettings().GetScrollBarSize();
    }

    aSz = CalcWindowSize( aSz );
    return aSz;
}

void ListBox::GetMaxVisColumnsAndLines( sal_uInt16& rnCols, sal_uInt16& rnLines ) const
{
    float nCharWidth = approximate_char_width();
    if ( !IsDropDownBox() )
    {
        Size aOutSz = mpImplLB->GetMainWindow().GetOutputSizePixel();
        rnCols = (sal_uInt16) (aOutSz.Width()/nCharWidth);
        rnLines = (sal_uInt16) (aOutSz.Height()/mpImplLB->GetEntryHeight());
    }
    else
    {
        Size aOutSz = mpImplWin->GetOutputSizePixel();
        rnCols = (sal_uInt16) (aOutSz.Width()/nCharWidth);
        rnLines = 1;
    }
}

IMPL_LINK( ListBox, ImplUserDrawHdl, UserDrawEvent*, pEvent )
{
    UserDraw( *pEvent );
    return 1;
}

void ListBox::UserDraw( const UserDrawEvent& )
{
}

void ListBox::DrawEntry( const UserDrawEvent& rEvt, bool bDrawImage, bool bDrawText, bool bDrawTextAtImagePos )
{
    if ( rEvt.GetDevice() == &mpImplLB->GetMainWindow() )
        mpImplLB->GetMainWindow().DrawEntry( rEvt.GetItemId(), bDrawImage, bDrawText, bDrawTextAtImagePos );
    else if ( rEvt.GetDevice() == mpImplWin )
        mpImplWin->DrawEntry( bDrawImage, bDrawText, bDrawTextAtImagePos );
}

void ListBox::SetUserItemSize( const Size& rSz )
{
    mpImplLB->GetMainWindow().SetUserItemSize( rSz );
    if ( mpImplWin )
        mpImplWin->SetUserItemSize( rSz );
}

void ListBox::EnableUserDraw( bool bUserDraw )
{
    mpImplLB->GetMainWindow().EnableUserDraw( bUserDraw );
    if ( mpImplWin )
        mpImplWin->EnableUserDraw( bUserDraw );
}

void ListBox::SetReadOnly( bool bReadOnly )
{
    if ( mpImplLB->IsReadOnly() != bReadOnly )
    {
        mpImplLB->SetReadOnly( bReadOnly );
        StateChanged( STATE_CHANGE_READONLY );
    }
}

bool ListBox::IsReadOnly() const
{
    return mpImplLB->IsReadOnly();
}

void ListBox::SetSeparatorPos( sal_Int32 n )
{
    mpImplLB->SetSeparatorPos( n );
}

sal_Int32 ListBox::GetSeparatorPos() const
{
    return mpImplLB->GetSeparatorPos();
}

sal_uInt16 ListBox::GetDisplayLineCount() const
{
    return mpImplLB->GetDisplayLineCount();
}

void ListBox::EnableMirroring()
{
    mpImplLB->EnableMirroring();
}

Rectangle ListBox::GetDropDownPosSizePixel() const
{
    return mpFloatWin ? mpFloatWin->GetWindowExtentsRelative( const_cast<ListBox*>(this) ) : Rectangle();
}

const Wallpaper& ListBox::GetDisplayBackground() const
{
    // !!! Recursion does not occur because the ImplListBox is initialized by default
    // to a non-transparent color in Window::ImplInitData
    return mpImplLB->GetDisplayBackground();
}

void ListBox::setMaxWidthChars(sal_Int32 nWidth)
{
    if (nWidth != m_nMaxWidthChars)
    {
        m_nMaxWidthChars = nWidth;
        queue_resize();
    }
}

bool ListBox::set_property(const OString &rKey, const OString &rValue)
{
    if (rKey == "active")
        SelectEntryPos(rValue.toInt32());
    else if (rKey == "max-width-chars")
        setMaxWidthChars(rValue.toInt32());
    else
        return Control::set_property(rKey, rValue);
    return true;
}

void ListBox::SetEdgeBlending(bool bNew)
{
    if(mbEdgeBlending != bNew)
    {
        mbEdgeBlending = bNew;

        if(IsDropDownBox())
        {
            mpImplWin->Invalidate();
        }
        else
        {
            mpImplLB->Invalidate();
        }

        if(mpImplWin)
        {
            mpImplWin->SetEdgeBlending(GetEdgeBlending());
        }

        if(mpImplLB)
        {
            mpImplLB->SetEdgeBlending(GetEdgeBlending());
        }

        Invalidate();
    }
}

MultiListBox::MultiListBox( Window* pParent, WinBits nStyle ) :
    ListBox( WINDOW_MULTILISTBOX )
{
    ImplInit( pParent, nStyle );
    EnableMultiSelection( true );
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
