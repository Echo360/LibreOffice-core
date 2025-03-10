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

#include <tools/shl.hxx>
#include <vcl/builder.hxx>
#include <vcl/svapp.hxx>
#include <vcl/settings.hxx>
#include <sfx2/dialoghelper.hxx>
#include <svx/xtable.hxx>
#include <svx/xpool.hxx>
#include <svx/dialogs.hrc>
#include <accessibility.hrc>
#include <svx/dlgctrl.hxx>
#include <svx/dialmgr.hxx>
#include <tools/poly.hxx>
#include <vcl/region.hxx>
#include <vcl/gradient.hxx>
#include <vcl/hatch.hxx>
#include <com/sun/star/accessibility/AccessibleEventObject.hpp>
#include <com/sun/star/accessibility/AccessibleEventId.hpp>
#include <com/sun/star/accessibility/AccessibleStateType.hpp>
#include "svxpixelctlaccessiblecontext.hxx"
#include <svtools/colorcfg.hxx>
#include <svxrectctaccessiblecontext.hxx>
#include <com/sun/star/lang/XUnoTunnel.hpp>
#include <basegfx/point/b2dpoint.hxx>
#include <basegfx/polygon/b2dpolygon.hxx>
#include <svx/svdorect.hxx>
#include <svx/svdmodel.hxx>
#include <svx/svdopath.hxx>
#include <svx/sdr/contact/objectcontactofobjlistpainter.hxx>
#include <svx/sdr/contact/displayinfo.hxx>
#include <vcl/bmpacc.hxx>
#include <svx/xbtmpit.hxx>

#define OUTPUT_DRAWMODE_COLOR       (DRAWMODE_DEFAULT)
#define OUTPUT_DRAWMODE_CONTRAST    (DRAWMODE_SETTINGSLINE | DRAWMODE_SETTINGSFILL | DRAWMODE_SETTINGSTEXT | DRAWMODE_SETTINGSGRADIENT)

using namespace ::com::sun::star::uno;
using namespace ::com::sun::star::lang;
using namespace ::com::sun::star::accessibility;

// Control for display and selection of the corner points and
// mid point of an object

Bitmap& SvxRectCtl::GetRectBitmap( void )
{
    if( !pBitmap )
        InitRectBitmap();

    return *pBitmap;
}

SvxRectCtl::SvxRectCtl(Window* pParent, RECT_POINT eRpt,
    sal_uInt16 nBorder, sal_uInt16 nCircle, CTL_STYLE eStyle)
    : Control(pParent, WB_BORDER | WB_TABSTOP)
    , pAccContext(NULL)
    , nBorderWidth(nBorder)
    , nRadius(nCircle)
    , eDefRP(eRpt)
    , eCS(eStyle)
    , pBitmap(NULL)
    , m_nState(0)
    , mbCompleteDisable(false)
{
    SetMapMode(MAP_100TH_MM);
    Resize_Impl();
}

void SvxRectCtl::SetControlSettings(RECT_POINT eRpt, sal_uInt16 nBorder, sal_uInt16 nCircle, CTL_STYLE eStyle)
{
    nBorderWidth = nBorder;
    nRadius = nCircle;
    eDefRP = eRpt;
    eCS = eStyle;
    Resize_Impl();
}

Size SvxRectCtl::GetOptimalSize() const
{
    return LogicToPixel(Size(78, 39), MAP_APPFONT);
}

extern "C" SAL_DLLPUBLIC_EXPORT Window* SAL_CALL makeSvxRectCtl(Window *pParent, VclBuilder::stringmap &)
{
    return new SvxRectCtl(pParent);
}



SvxRectCtl::~SvxRectCtl()
{
    delete pBitmap;

    if( pAccContext )
        pAccContext->release();
}


void SvxRectCtl::Resize()
{
    Resize_Impl();
    Control::Resize();
}



void SvxRectCtl::Resize_Impl()
{
    aSize = GetOutputSize();

    switch( eCS )
    {
        case CS_RECT:
        case CS_ANGLE:
        case CS_SHADOW:
            aPtLT = Point( 0 + nBorderWidth,  0 + nBorderWidth );
            aPtMT = Point( aSize.Width() / 2, 0 + nBorderWidth );
            aPtRT = Point( aSize.Width() - nBorderWidth, 0 + nBorderWidth );

            aPtLM = Point( 0 + nBorderWidth,  aSize.Height() / 2 );
            aPtMM = Point( aSize.Width() / 2, aSize.Height() / 2 );
            aPtRM = Point( aSize.Width() - nBorderWidth, aSize.Height() / 2 );

            aPtLB = Point( 0 + nBorderWidth,    aSize.Height() - nBorderWidth );
            aPtMB = Point( aSize.Width() / 2,   aSize.Height() - nBorderWidth );
            aPtRB = Point( aSize.Width() - nBorderWidth, aSize.Height() - nBorderWidth );
        break;

        case CS_LINE:
            aPtLT = Point( 0 + 3 * nBorderWidth,  0 + nBorderWidth );
            aPtMT = Point( aSize.Width() / 2, 0 + nBorderWidth );
            aPtRT = Point( aSize.Width() - 3 * nBorderWidth, 0 + nBorderWidth );

            aPtLM = Point( 0 + 3 * nBorderWidth,  aSize.Height() / 2 );
            aPtMM = Point( aSize.Width() / 2, aSize.Height() / 2 );
            aPtRM = Point( aSize.Width() - 3 * nBorderWidth, aSize.Height() / 2 );

            aPtLB = Point( 0 + 3 * nBorderWidth,    aSize.Height() - nBorderWidth );
            aPtMB = Point( aSize.Width() / 2,   aSize.Height() - nBorderWidth );
            aPtRB = Point( aSize.Width() - 3 * nBorderWidth, aSize.Height() - nBorderWidth );
        break;
    }
    Reset();
    InitSettings( true, true );
}


void SvxRectCtl::InitRectBitmap( void )
{
    delete pBitmap;

    const StyleSettings&    rStyles = Application::GetSettings().GetStyleSettings();
    svtools::ColorConfig aColorConfig;

    pBitmap = new Bitmap( SVX_RES( RID_SVXCTRL_RECTBTNS ) );

    // set bitmap-colors
    Color   aColorAry1[7];
    Color   aColorAry2[7];
    aColorAry1[0] = Color( 0xC0, 0xC0, 0xC0 );  // light-gray
    aColorAry1[1] = Color( 0xFF, 0xFF, 0x00 );  // yellow
    aColorAry1[2] = Color( 0xFF, 0xFF, 0xFF );  // white
    aColorAry1[3] = Color( 0x80, 0x80, 0x80 );  // dark-gray
    aColorAry1[4] = Color( 0x00, 0x00, 0x00 );  // black
    aColorAry1[5] = Color( 0x00, 0xFF, 0x00 );  // green
    aColorAry1[6] = Color( 0x00, 0x00, 0xFF );  // blue
    aColorAry2[0] = rStyles.GetDialogColor();       // background
    aColorAry2[1] = rStyles.GetWindowColor();
    aColorAry2[2] = rStyles.GetLightColor();
    aColorAry2[3] = rStyles.GetShadowColor();
    aColorAry2[4] = rStyles.GetDarkShadowColor();
    aColorAry2[5] = Color( aColorConfig.GetColorValue( svtools::FONTCOLOR ).nColor );
    aColorAry2[6] = rStyles.GetDialogColor();

#ifdef DBG_UTIL
    static bool     bModify = false;
    bool&           rModify = bModify;
    if( rModify )
    {
        static int      n = 0;
        static sal_uInt8    r = 0xFF;
        static sal_uInt8    g = 0x00;
        static sal_uInt8    b = 0xFF;
        int&            rn = n;
        sal_uInt8&          rr = r;
        sal_uInt8&          rg = g;
        sal_uInt8&          rb = b;
        aColorAry2[ rn ] = Color( rr, rg, rb );
    }
#endif

    pBitmap->Replace( aColorAry1, aColorAry2, 7, NULL );
}



void SvxRectCtl::InitSettings( bool bForeground, bool bBackground )
{
    const StyleSettings& rStyleSettings = Application::GetSettings().GetStyleSettings();

    if( bForeground )
    {
        svtools::ColorConfig aColorConfig;
        Color aTextColor( aColorConfig.GetColorValue( svtools::FONTCOLOR ).nColor );

        if ( IsControlForeground() )
            aTextColor = GetControlForeground();
        SetTextColor( aTextColor );
    }

    if( bBackground )
    {
        if ( IsControlBackground() )
            SetBackground( GetControlBackground() );
        else
            SetBackground( rStyleSettings.GetWindowColor() );
    }

    delete pBitmap;
    pBitmap = NULL;     // forces new creating of bitmap

    Invalidate();
}

// The clicked rectangle (3 x 3) is determined and the parent (dialog)
// is notified that the item was changed
void SvxRectCtl::MouseButtonDown( const MouseEvent& rMEvt )
{
    // CompletelyDisabled() added to have a disabled state for SvxRectCtl
    if(!IsCompletelyDisabled())
    {
        Point aPtLast = aPtNew;

        aPtNew = GetApproxLogPtFromPixPt( rMEvt.GetPosPixel() );

        if( aPtNew == aPtMM && ( eCS == CS_SHADOW || eCS == CS_ANGLE ) )
        {
            aPtNew = aPtLast;
        }
        else
        {
            Invalidate( Rectangle( aPtLast - Point( nRadius, nRadius ),
                                   aPtLast + Point( nRadius, nRadius ) ) );
            Invalidate( Rectangle( aPtNew - Point( nRadius, nRadius ),
                                   aPtNew + Point( nRadius, nRadius ) ) );
            eRP = GetRPFromPoint( aPtNew );

            SetActualRP( eRP );

            Window *pTabPage = getNonLayoutParent(this);
            if (pTabPage && WINDOW_TABPAGE == pTabPage->GetType())
                ( (SvxTabPage*)pTabPage)->PointChanged( this, eRP );
        }
    }
}

void SvxRectCtl::KeyInput( const KeyEvent& rKeyEvt )
{
    // CompletelyDisabled() added to have a disabled state for SvxRectCtl
    if(!IsCompletelyDisabled())
    {
        RECT_POINT eNewRP = eRP;
        bool bUseMM = (eCS != CS_SHADOW) && (eCS != CS_ANGLE);

        switch( rKeyEvt.GetKeyCode().GetCode() )
        {
            case KEY_DOWN:
            {
                if( !(m_nState & CS_NOVERT) )
                    switch( eNewRP )
                    {
                        case RP_LT: eNewRP = RP_LM; break;
                        case RP_MT: eNewRP = bUseMM ? RP_MM : RP_MB; break;
                        case RP_RT: eNewRP = RP_RM; break;
                        case RP_LM: eNewRP = RP_LB; break;
                        case RP_MM: eNewRP = RP_MB; break;
                        case RP_RM: eNewRP = RP_RB; break;
                        default: ; //prevent warning
                    }
            }
            break;
            case KEY_UP:
            {
                if( !(m_nState & CS_NOVERT) )
                    switch( eNewRP )
                    {
                        case RP_LM: eNewRP = RP_LT; break;
                        case RP_MM: eNewRP = RP_MT; break;
                        case RP_RM: eNewRP = RP_RT; break;
                        case RP_LB: eNewRP = RP_LM; break;
                        case RP_MB: eNewRP = bUseMM ? RP_MM : RP_MT; break;
                        case RP_RB: eNewRP = RP_RM; break;
                        default: ; //prevent warning
                    }
            }
            break;
            case KEY_LEFT:
            {
                if( !(m_nState & CS_NOHORZ) )
                    switch( eNewRP )
                    {
                        case RP_MT: eNewRP = RP_LT; break;
                        case RP_RT: eNewRP = RP_MT; break;
                        case RP_MM: eNewRP = RP_LM; break;
                        case RP_RM: eNewRP = bUseMM ? RP_MM : RP_LM; break;
                        case RP_MB: eNewRP = RP_LB; break;
                        case RP_RB: eNewRP = RP_MB; break;
                        default: ; //prevent warning
                    }
            }
            break;
            case KEY_RIGHT:
            {
                if( !(m_nState & CS_NOHORZ) )
                    switch( eNewRP )
                    {
                        case RP_LT: eNewRP = RP_MT; break;
                        case RP_MT: eNewRP = RP_RT; break;
                        case RP_LM: eNewRP = bUseMM ? RP_MM : RP_RM; break;
                        case RP_MM: eNewRP = RP_RM; break;
                        case RP_LB: eNewRP = RP_MB; break;
                        case RP_MB: eNewRP = RP_RB; break;
                        default: ; //prevent warning
                    }
            }
            break;
            default:
                Control::KeyInput( rKeyEvt );
                return;
        }
        if( eNewRP != eRP )
        {
            SetActualRP( eNewRP );

            Window *pTabPage = getNonLayoutParent(this);
            if (pTabPage && WINDOW_TABPAGE == pTabPage->GetType())
                ((SvxTabPage*) pTabPage)->PointChanged(this, eRP);

            SetFocusRect();
        }
    }
}



void SvxRectCtl::StateChanged( StateChangedType nType )
{
    if ( nType == STATE_CHANGE_CONTROLFOREGROUND )
        InitSettings( true, false );
    else if ( nType == STATE_CHANGE_CONTROLBACKGROUND )
        InitSettings( false, true );

    Window::StateChanged( nType );
}



void SvxRectCtl::DataChanged( const DataChangedEvent& rDCEvt )
{
    if ( ( rDCEvt.GetType() == DATACHANGED_SETTINGS ) && ( rDCEvt.GetFlags() & SETTINGS_STYLE ) )
        InitSettings( true, true );
    else
        Window::DataChanged( rDCEvt );
}

// the control (rectangle with 9 circles)

void SvxRectCtl::Paint( const Rectangle& )
{
    Point   aPtDiff( PixelToLogic( Point( 1, 1 ) ) );

    const StyleSettings& rStyles = Application::GetSettings().GetStyleSettings();

    SetLineColor( rStyles.GetDialogColor() );
    SetFillColor( rStyles.GetDialogColor() );
    DrawRect( Rectangle( Point(0,0), GetOutputSize() ) );

    if( IsEnabled() )
        SetLineColor( rStyles.GetLabelTextColor() );
    else
        SetLineColor( rStyles.GetShadowColor() );

    SetFillColor();

    switch( eCS )
    {

        case CS_RECT:
        case CS_SHADOW:
            if( !IsEnabled() )
            {
                Color aOldCol = GetLineColor();
                SetLineColor( rStyles.GetLightColor() );
                DrawRect( Rectangle( aPtLT + aPtDiff, aPtRB + aPtDiff ) );
                SetLineColor( aOldCol );
            }
            DrawRect( Rectangle( aPtLT, aPtRB ) );
        break;

        case CS_LINE:
            if( !IsEnabled() )
            {
                Color aOldCol = GetLineColor();
                SetLineColor( rStyles.GetLightColor() );
                DrawLine( aPtLM - Point( 2 * nBorderWidth, 0) + aPtDiff,
                          aPtRM + Point( 2 * nBorderWidth, 0 ) + aPtDiff );
                SetLineColor( aOldCol );
            }
            DrawLine( aPtLM - Point( 2 * nBorderWidth, 0),
                      aPtRM + Point( 2 * nBorderWidth, 0 ) );
        break;

        case CS_ANGLE:
            if( !IsEnabled() )
            {
                Color aOldCol = GetLineColor();
                SetLineColor( rStyles.GetLightColor() );
                DrawLine( aPtLT + aPtDiff, aPtRB + aPtDiff );
                DrawLine( aPtLB + aPtDiff, aPtRT + aPtDiff );
                DrawLine( aPtLM + aPtDiff, aPtRM + aPtDiff );
                DrawLine( aPtMT + aPtDiff, aPtMB + aPtDiff );
                SetLineColor( aOldCol );
            }
            DrawLine( aPtLT, aPtRB );
            DrawLine( aPtLB, aPtRT );
            DrawLine( aPtLM, aPtRM );
            DrawLine( aPtMT, aPtMB );
        break;

        default:
            break;
    }
    SetFillColor( GetBackground().GetColor() );

    Size aBtnSize( 11, 11 );
    Size aDstBtnSize(  PixelToLogic( aBtnSize ) );
    Point aToCenter( aDstBtnSize.Width() >> 1, aDstBtnSize.Height() >> 1);
    Point aBtnPnt1( IsEnabled()?0:22,0 );
    Point aBtnPnt2( 11,0 );
    Point aBtnPnt3( 22,0 );

    bool bNoHorz = (m_nState & CS_NOHORZ) != 0;
    bool bNoVert = (m_nState & CS_NOVERT) != 0;

    Bitmap&         rBitmap = GetRectBitmap();

    // CompletelyDisabled() added to have a disabled state for SvxRectCtl
    if(IsCompletelyDisabled())
    {
        DrawBitmap( aPtLT - aToCenter, aDstBtnSize, aBtnPnt3, aBtnSize, rBitmap );
        DrawBitmap( aPtMT - aToCenter, aDstBtnSize, aBtnPnt3, aBtnSize, rBitmap );
        DrawBitmap( aPtRT - aToCenter, aDstBtnSize, aBtnPnt3, aBtnSize, rBitmap );
        DrawBitmap( aPtLM - aToCenter, aDstBtnSize, aBtnPnt3, aBtnSize, rBitmap );
        if( eCS == CS_RECT || eCS == CS_LINE )
            DrawBitmap( aPtMM - aToCenter, aDstBtnSize, aBtnPnt3, aBtnSize, rBitmap );
        DrawBitmap( aPtRM - aToCenter, aDstBtnSize, aBtnPnt3, aBtnSize, rBitmap );
        DrawBitmap( aPtLB - aToCenter, aDstBtnSize, aBtnPnt3, aBtnSize, rBitmap );
        DrawBitmap( aPtMB - aToCenter, aDstBtnSize, aBtnPnt3, aBtnSize, rBitmap );
        DrawBitmap( aPtRB - aToCenter, aDstBtnSize, aBtnPnt3, aBtnSize, rBitmap );
    }
    else
    {
        DrawBitmap( aPtLT - aToCenter, aDstBtnSize, (bNoHorz || bNoVert)?aBtnPnt3:aBtnPnt1, aBtnSize, rBitmap );
        DrawBitmap( aPtMT - aToCenter, aDstBtnSize, bNoVert?aBtnPnt3:aBtnPnt1, aBtnSize, rBitmap );
        DrawBitmap( aPtRT - aToCenter, aDstBtnSize, (bNoHorz || bNoVert)?aBtnPnt3:aBtnPnt1, aBtnSize, rBitmap );

        DrawBitmap( aPtLM - aToCenter, aDstBtnSize, bNoHorz?aBtnPnt3:aBtnPnt1, aBtnSize, rBitmap );

        // Center for rectangle and line
        if( eCS == CS_RECT || eCS == CS_LINE )
            DrawBitmap( aPtMM - aToCenter, aDstBtnSize, aBtnPnt1, aBtnSize, rBitmap );

        DrawBitmap( aPtRM - aToCenter, aDstBtnSize, bNoHorz?aBtnPnt3:aBtnPnt1, aBtnSize, rBitmap );

        DrawBitmap( aPtLB - aToCenter, aDstBtnSize, (bNoHorz || bNoVert)?aBtnPnt3:aBtnPnt1, aBtnSize, rBitmap );
        DrawBitmap( aPtMB - aToCenter, aDstBtnSize, bNoVert?aBtnPnt3:aBtnPnt1, aBtnSize, rBitmap );
        DrawBitmap( aPtRB - aToCenter, aDstBtnSize, (bNoHorz || bNoVert)?aBtnPnt3:aBtnPnt1, aBtnSize, rBitmap );
    }

    // draw active button, avoid center pos for angle
    // CompletelyDisabled() added to have a disabled state for SvxRectCtl
    if(!IsCompletelyDisabled())
    {
        if( IsEnabled() && (eCS != CS_ANGLE || aPtNew != aPtMM) )
        {
            Point       aCenterPt( aPtNew );
            aCenterPt -= aToCenter;

            DrawBitmap( aCenterPt, aDstBtnSize, aBtnPnt2, aBtnSize, rBitmap );
        }
    }
}

// Convert RECT_POINT Point

Point SvxRectCtl::GetPointFromRP( RECT_POINT _eRP) const
{
    switch( _eRP )
    {
        case RP_LT: return aPtLT;
        case RP_MT: return aPtMT;
        case RP_RT: return aPtRT;
        case RP_LM: return aPtLM;
        case RP_MM: return aPtMM;
        case RP_RM: return aPtRM;
        case RP_LB: return aPtLB;
        case RP_MB: return aPtMB;
        case RP_RB: return aPtRB;
    }
    return( aPtMM ); // default
}


void SvxRectCtl::SetFocusRect( const Rectangle* pRect )
{
    HideFocus();

    if( pRect )
        ShowFocus( *pRect );
    else
        ShowFocus( CalculateFocusRectangle() );
}

Point SvxRectCtl::SetActualRPWithoutInvalidate( RECT_POINT eNewRP )
{
    Point aPtLast = aPtNew;
    aPtNew = GetPointFromRP( eNewRP );

    if( (m_nState & CS_NOHORZ) != 0 )
        aPtNew.X() = aPtMM.X();

    if( (m_nState & CS_NOVERT) != 0 )
        aPtNew.Y() = aPtMM.Y();

    // fdo#74751 this fix reverse base point on RTL UI.
    bool bRTL = Application::GetSettings().GetLayoutRTL();
    eNewRP = GetRPFromPoint( aPtNew, bRTL );

    eDefRP = eNewRP;
    eRP = eNewRP;

    return aPtLast;
}

void SvxRectCtl::GetFocus()
{
    SetFocusRect();
    // Send the accessible focused event
    Control::GetFocus();
    // Send accessibility event.
    if(pAccContext)
    {
        pAccContext->FireChildFocus(GetActualRP());
    }
}


void SvxRectCtl::LoseFocus()
{
    HideFocus();
}


Point SvxRectCtl::GetApproxLogPtFromPixPt( const Point& rPt ) const
{
    Point   aPt = PixelToLogic( rPt );
    long    x;
    long    y;

    if( ( m_nState & CS_NOHORZ ) == 0 )
    {
        if( aPt.X() < aSize.Width() / 3 )
            x = aPtLT.X();
        else if( aPt.X() < aSize.Width() * 2 / 3 )
            x = aPtMM.X();
        else
            x = aPtRB.X();
    }
    else
        x = aPtMM.X();

    if( ( m_nState & CS_NOVERT ) == 0 )
    {
        if( aPt.Y() < aSize.Height() / 3 )
            y = aPtLT.Y();
        else if( aPt.Y() < aSize.Height() * 2 / 3 )
            y = aPtMM.Y();
        else
            y = aPtRB.Y();
    }
    else
            y = aPtMM.Y();

    return Point( x, y );
}


// Converts Point in RECT_POINT

RECT_POINT SvxRectCtl::GetRPFromPoint( Point aPt, bool bRTL ) const
{
    RECT_POINT rPoint = RP_MM;  // default

    if     ( aPt == aPtLT) rPoint = bRTL ? RP_RT : RP_LT;
    else if( aPt == aPtMT) rPoint = RP_MT;
    else if( aPt == aPtRT) rPoint = bRTL ? RP_LT : RP_RT;
    else if( aPt == aPtLM) rPoint = bRTL ? RP_RM : RP_LM;
    else if( aPt == aPtRM) rPoint = bRTL ? RP_LM : RP_RM;
    else if( aPt == aPtLB) rPoint = bRTL ? RP_RB : RP_LB;
    else if( aPt == aPtMB) rPoint = RP_MB;
    else if( aPt == aPtRB) rPoint = bRTL ? RP_LB : RP_RB;

    return rPoint;
}

// Resets to the original state of the control

void SvxRectCtl::Reset()
{
    aPtNew = GetPointFromRP( eDefRP );
    eRP = eDefRP;
    Invalidate();
}

// Returns the currently selected RECT_POINT


void SvxRectCtl::SetActualRP( RECT_POINT eNewRP )
{
    Point aPtLast( SetActualRPWithoutInvalidate( eNewRP ) );

    Invalidate( Rectangle( aPtLast - Point( nRadius, nRadius ), aPtLast + Point( nRadius, nRadius ) ) );
    Invalidate( Rectangle( aPtNew - Point( nRadius, nRadius ), aPtNew + Point( nRadius, nRadius ) ) );

    // notify accessibility object about change
    if( pAccContext )
        pAccContext->selectChild( eNewRP /* MT, bFireFocus */ );
}

void SvxRectCtl::SetState( CTL_STATE nState )
{
    m_nState = nState;

    Point aPtLast( GetPointFromRP( eRP ) );
    Point _aPtNew( aPtLast );

    if( (m_nState & CS_NOHORZ) != 0 )
        _aPtNew.X() = aPtMM.X();

    if( (m_nState & CS_NOVERT) != 0 )
        _aPtNew.Y() = aPtMM.Y();

    eRP = GetRPFromPoint( _aPtNew );
    Invalidate();

    Window *pTabPage = getNonLayoutParent(this);
    if (pTabPage && WINDOW_TABPAGE == pTabPage->GetType())
        ((SvxTabPage*)pTabPage)->PointChanged(this, eRP);
}

sal_uInt8 SvxRectCtl::GetNumOfChildren( void ) const
{
    return ( eCS == CS_ANGLE )? 8 : 9;
}

Rectangle SvxRectCtl::CalculateFocusRectangle( void ) const
{
    Size        aDstBtnSize( PixelToLogic( Size( 15, 15 ) ) );
    return Rectangle( aPtNew - Point( aDstBtnSize.Width() >> 1, aDstBtnSize.Height() >> 1 ), aDstBtnSize );
}

Rectangle SvxRectCtl::CalculateFocusRectangle( RECT_POINT eRectPoint ) const
{
    Rectangle   aRet;
    RECT_POINT  eOldRectPoint = GetActualRP();

    if( eOldRectPoint == eRectPoint )
        aRet = CalculateFocusRectangle();
    else
    {
        SvxRectCtl* pThis = const_cast< SvxRectCtl* >( this );

        pThis->SetActualRPWithoutInvalidate( eRectPoint );      // no invalidation because it's only temporary!
        aRet = CalculateFocusRectangle();

        pThis->SetActualRPWithoutInvalidate( eOldRectPoint );   // no invalidation because nothing has changed!
    }

    return aRet;
}

Reference< XAccessible > SvxRectCtl::CreateAccessible()
{
    Window*                     pParent = GetAccessibleParentWindow();

    DBG_ASSERT( pParent, "-SvxRectCtl::CreateAccessible(): No Parent!" );

    Reference< XAccessible >    xAccParent  = pParent->GetAccessible();
    if( xAccParent.is() )
    {
        pAccContext = new SvxRectCtlAccessibleContext( xAccParent, *this );
        pAccContext->acquire();

        SetActualRP( GetActualRP() );

        return pAccContext;
    }
    else
        return Reference< XAccessible >();
}

RECT_POINT SvxRectCtl::GetApproxRPFromPixPt( const ::com::sun::star::awt::Point& r ) const
{
    return GetRPFromPoint( GetApproxLogPtFromPixPt( Point( r.X, r.Y ) ) );
}

// CompletelyDisabled() added to have a disabled state for SvxRectCtl
void SvxRectCtl::DoCompletelyDisable(bool bNew)
{
    mbCompleteDisable = bNew;
    Invalidate();
}

void SvxRectCtl::SetCS(CTL_STYLE eNew)
{
    eCS = eNew;
}

// Control for editing bitmaps

::com::sun::star::uno::Reference< ::com::sun::star::accessibility::XAccessible > SvxPixelCtl::CreateAccessible()
{
    if(!m_xAccess.is())
    {
        m_xAccess = m_pAccess =  new SvxPixelCtlAccessible(*this);
    }
    return m_xAccess;
}

//Logic Pixel
long SvxPixelCtl::PointToIndex(const Point &aPt) const
{
    long nX = aPt.X() * nLines / aRectSize.Width();
    long nY = aPt.Y() * nLines / aRectSize.Height();

    return nX + nY * nLines ;
}

Point SvxPixelCtl::IndexToPoint(long nIndex) const
{
    DBG_ASSERT(nIndex >= 0 && nIndex < nSquares ," Check Index");

    sal_Int32 nXIndex = nIndex % nLines;
    sal_Int32 nYIndex = nIndex / nLines;

    Point aPtTl;
    aPtTl.Y() = aRectSize.Height() * nYIndex / nLines + 1;
    aPtTl.X() = aRectSize.Width() * nXIndex / nLines + 1;

    return aPtTl;
}

long SvxPixelCtl::GetFoucsPosIndex() const
{
    return aFocusPosition.getX() + aFocusPosition.getY() * nLines ;
}

long SvxPixelCtl::ShowPosition( const Point &pt)
{
    Point aPt = PixelToLogic( pt );

    sal_Int32 nX = aPt.X() * nLines / aRectSize.Width();
    sal_Int32 nY = aPt.Y() * nLines / aRectSize.Height();

    ChangePixel( nX + nY * nLines );

    //Solution:Set new focus position and repaint
    //Invalidate( Rectangle( aPtTl, aPtBr ) );
    aFocusPosition.setX(nX);
    aFocusPosition.setY(nY);
    Invalidate(Rectangle(Point(0,0),aRectSize));

    Window *pTabPage = getNonLayoutParent(this);
    if (pTabPage && WINDOW_TABPAGE == pTabPage->GetType())
        ( (SvxTabPage*)pTabPage)->PointChanged( this, RP_MM ); // RectPoint ist dummy

    return GetFoucsPosIndex();

}

SvxPixelCtl::SvxPixelCtl(Window* pParent, sal_uInt16 nNumber)
    : Control(pParent, WB_BORDER)
    , nLines(nNumber)
    , bPaintable(true)
    , aFocusPosition(0,0)
{
    assert(nLines); // can't have no lines
    SetPixelColor( Color( COL_BLACK ) );
    SetBackgroundColor( Color( COL_WHITE ) );
    SetLineColor( Application::GetSettings().GetStyleSettings().GetShadowColor() );

    nSquares = nLines * nLines;
    pPixel = new sal_uInt16[ nSquares ];
    memset(pPixel, 0, nSquares * sizeof(sal_uInt16));
    m_pAccess=NULL;
}

void SvxPixelCtl::Resize()
{
    Control::Resize();
    aRectSize = GetOutputSize();
}

Size SvxPixelCtl::GetOptimalSize() const
{
    return LogicToPixel(Size(72, 72), MAP_APPFONT);
}

extern "C" SAL_DLLPUBLIC_EXPORT Window* SAL_CALL makeSvxPixelCtl(Window *pParent, VclBuilder::stringmap&)
{
    return new SvxPixelCtl(pParent, 8);
}
// Destructor dealocating the dynamic array

SvxPixelCtl::~SvxPixelCtl( )
{
    delete []pPixel;
}

// Changes the foreground or Background color

void SvxPixelCtl::ChangePixel( sal_uInt16 nPixel )
{
    if( *( pPixel + nPixel) == 0 )
        *( pPixel + nPixel) = 1; //  could be extended to more colors
    else
        *( pPixel + nPixel) = 0;
}

// The clicked rectangle is identified, to change its color

void SvxPixelCtl::MouseButtonDown( const MouseEvent& rMEvt )
{
    if (!aRectSize.Width() || !aRectSize.Height())
        return;

    //Grab focus when click in window
    if (!HasFocus())
    {
        GrabFocus();
    }

    long nIndex = ShowPosition(rMEvt.GetPosPixel());

    if(m_pAccess)
    {
        m_pAccess->NotifyChild(nIndex,true, true);
    }
}

// Draws the Control (Rectangle with nine circles)

void SvxPixelCtl::Paint( const Rectangle& )
{
    if (!aRectSize.Width() || !aRectSize.Height())
        return;

    sal_uInt16  i, j, nTmp;
    Point   aPtTl, aPtBr;

    if( bPaintable )
    {
        // Draw lines
        Control::SetLineColor( aLineColor );
        for( i = 1; i < nLines; i++)
        {
            // horizontal
            nTmp = (sal_uInt16) ( aRectSize.Height() * i / nLines );
            DrawLine( Point( 0, nTmp ), Point( aRectSize.Width(), nTmp ) );
            // vertically
            nTmp = (sal_uInt16) ( aRectSize.Width() * i / nLines );
            DrawLine( Point( nTmp, 0 ), Point( nTmp, aRectSize.Height() ) );
        }

        //Draw Rectangles (squares)
        Control::SetLineColor();
        sal_uInt16 nLastPixel = *pPixel ? 0 : 1;

        for( i = 0; i < nLines; i++)
        {
            aPtTl.Y() = aRectSize.Height() * i / nLines + 1;
            aPtBr.Y() = aRectSize.Height() * (i + 1) / nLines - 1;

            for( j = 0; j < nLines; j++)
            {
                aPtTl.X() = aRectSize.Width() * j / nLines + 1;
                aPtBr.X() = aRectSize.Width() * (j + 1) / nLines - 1;

                if ( *( pPixel + i * nLines + j ) != nLastPixel )
                {
                    nLastPixel = *( pPixel + i * nLines + j );
                    // Change color: 0 -> Background color
                    SetFillColor( nLastPixel ? aPixelColor : aBackgroundColor );
                }
                DrawRect( Rectangle( aPtTl, aPtBr ) );
            }
        }
        //Draw visual focus when has focus
        if( HasFocus() )
        {
            ShowFocus(implCalFocusRect(aFocusPosition));
        }
    } // bPaintable
    else
    {
        SetBackground( Wallpaper( Color( COL_LIGHTGRAY ) ) );
        Control::SetLineColor( Color( COL_LIGHTRED ) );
        DrawLine( Point( 0, 0 ), Point( aRectSize.Width(), aRectSize.Height() ) );
        DrawLine( Point( 0, aRectSize.Height() ), Point( aRectSize.Width(), 0 ) );
    }
}

//Calculate visual focus rectangle via focus position
Rectangle SvxPixelCtl::implCalFocusRect( const Point& aPosition )
{
    long nLeft,nTop,nRight,nBottom;
    long i,j;
    i = aPosition.Y();
    j = aPosition.X();
    nLeft = aRectSize.Width() * j / nLines + 1;
    nRight = aRectSize.Width() * (j + 1) / nLines - 1;
    nTop = aRectSize.Height() * i / nLines + 1;
    nBottom = aRectSize.Height() * (i + 1) / nLines - 1;
    return Rectangle(nLeft,nTop,nRight,nBottom);
}

//Solution:Keyboard function
void SvxPixelCtl::KeyInput( const KeyEvent& rKEvt )
{
    KeyCode aKeyCode = rKEvt.GetKeyCode();
    sal_uInt16 nCode = aKeyCode.GetCode();
    bool bIsMod = aKeyCode.IsShift() || aKeyCode.IsMod1() || aKeyCode.IsMod2();

    if( !bIsMod )
    {
        Point pRepaintPoint( aRectSize.Width() *( aFocusPosition.getX() - 1)/ nLines - 1,
                             aRectSize.Height() *( aFocusPosition.getY() - 1)/ nLines -1
                            );
        Size  mRepaintSize( aRectSize.Width() *3/ nLines + 2,aRectSize.Height() *3/ nLines + 2);
        Rectangle mRepaintRect( pRepaintPoint, mRepaintSize );
        bool bFocusPosChanged=false;
        switch(nCode)
        {
            case KEY_LEFT:
                if((aFocusPosition.getX() >= 1))
                {
                    aFocusPosition.setX( aFocusPosition.getX() - 1 );
                    Invalidate(mRepaintRect);
                    bFocusPosChanged=true;
                }
                break;
            case KEY_RIGHT:
                if( aFocusPosition.getX() < (nLines - 1) )
                {
                    aFocusPosition.setX( aFocusPosition.getX() + 1 );
                    Invalidate(mRepaintRect);
                    bFocusPosChanged=true;
                }
                break;
            case KEY_UP:
                if((aFocusPosition.getY() >= 1))
                {
                    aFocusPosition.setY( aFocusPosition.getY() - 1 );
                    Invalidate(mRepaintRect);
                    bFocusPosChanged=true;
                }
                break;
            case KEY_DOWN:
                if( aFocusPosition.getY() < ( nLines - 1 ) )
                {
                    aFocusPosition.setY( aFocusPosition.getY() + 1 );
                    Invalidate(mRepaintRect);
                    bFocusPosChanged=true;
                }
                break;
            case KEY_SPACE:
                ChangePixel( sal_uInt16(aFocusPosition.getX() + aFocusPosition.getY() * nLines) );
                Invalidate( implCalFocusRect(aFocusPosition) );
                break;
            default:
                Control::KeyInput( rKEvt );
                return;
        }
        if(m_xAccess.is())
        {
            long nIndex = GetFoucsPosIndex();
            switch(nCode)
            {
            case KEY_LEFT:
            case KEY_RIGHT:
            case KEY_UP:
            case KEY_DOWN:
                if (bFocusPosChanged)
                {
                    m_pAccess->NotifyChild(nIndex,false,false);
                }
                break;
            case KEY_SPACE:
                m_pAccess->NotifyChild(nIndex,false,true);
                break;
            default:
                break;
            }
        }
    }
    else
    {
        Control::KeyInput( rKEvt );
    }
}

//Draw focus when get focus
void SvxPixelCtl::GetFocus()
{
    Invalidate(implCalFocusRect(aFocusPosition));

    if(m_pAccess)
    {
        m_pAccess->NotifyChild(GetFoucsPosIndex(),true,false);
    }

    Control::GetFocus();
}

//Hide focus when lose focus
void SvxPixelCtl::LoseFocus()
{
    HideFocus();
    if (m_pAccess)
    {
        m_pAccess->LoseFocus();
    }
    Control::LoseFocus();
}

void SvxPixelCtl::SetXBitmap( const BitmapEx& rBitmapEx )
{
    BitmapColor aBack;
    BitmapColor aFront;

    if(isHistorical8x8(rBitmapEx, aBack, aFront))
    {
        Bitmap aBitmap(rBitmapEx.GetBitmap());
        BitmapReadAccess* pRead = aBitmap.AcquireReadAccess();

        aBackgroundColor = aBack;
        aPixelColor = aFront;

        for(sal_uInt16 i(0); i < nSquares; i++)
        {
            const BitmapColor aColor(pRead->GetColor(i/8, i%8));

            if(aColor == aBack)
            {
                *( pPixel + i ) = 0;
            }
            else
            {
                *( pPixel + i ) = 1;
            }
        }

        aBitmap.ReleaseAccess(pRead);
    }
}

// Returns a specific pixel

sal_uInt16 SvxPixelCtl::GetBitmapPixel( const sal_uInt16 nPixel )
{
    return( *( pPixel + nPixel ) );
}

// Resets to the original state of the control

void SvxPixelCtl::Reset()
{
    // clear pixel area
    memset(pPixel, 0, nSquares * sizeof(sal_uInt16));
    Invalidate();
}

// Constructor: BitmapCtl for SvxPixelCtl

SvxBitmapCtl::SvxBitmapCtl( Window* /*pParent*/, const Size& rSize )
    : aSize(rSize)
    , nLines(0)
    , pBmpArray(NULL)
{
}

SvxBitmapCtl::~SvxBitmapCtl()
{
}

// BitmapCtl: Returns the Bitmap

BitmapEx SvxBitmapCtl::GetBitmapEx()
{
    const Bitmap aRetval(createHistorical8x8FromArray(pBmpArray, aPixelColor, aBackgroundColor));

    return BitmapEx(aRetval);
}

extern "C" SAL_DLLPUBLIC_EXPORT Window* SAL_CALL makeColorLB(Window *pParent, VclBuilder::stringmap &rMap)
{
    bool bDropdown = VclBuilder::extractDropdown(rMap);
    WinBits nWinBits = WB_LEFT|WB_VCENTER|WB_3DLOOK|WB_SIMPLEMODE|WB_TABSTOP;
    if (bDropdown)
        nWinBits |= WB_DROPDOWN;
    OString sBorder = VclBuilder::extractCustomProperty(rMap);
    if (!sBorder.isEmpty())
        nWinBits |= WB_BORDER;
    ColorLB *pListBox = new ColorLB(pParent, nWinBits);
    pListBox->EnableAutoSize(true);
    return pListBox;
}

// Fills the Listbox with color and strings

void ColorLB::Fill( const XColorListRef &pColorTab )
{
    if( !pColorTab.is() )
        return;

    long nCount = pColorTab->Count();
    XColorEntry* pEntry;
    SetUpdateMode( false );

    for( long i = 0; i < nCount; i++ )
    {
        pEntry = pColorTab->GetColor( i );
        InsertEntry( pEntry->GetColor(), pEntry->GetName() );
    }

    AdaptDropDownLineCountToMaximum();
    SetUpdateMode( true );
}

/************************************************************************/

void ColorLB::Append( const XColorEntry& rEntry )
{
    InsertEntry( rEntry.GetColor(), rEntry.GetName() );
    AdaptDropDownLineCountToMaximum();
}

/************************************************************************/

void ColorLB::Modify( const XColorEntry& rEntry, sal_Int32 nPos )
{
    RemoveEntry( nPos );
    InsertEntry( rEntry.GetColor(), rEntry.GetName(), nPos );
}

// Fills the Listbox with color and strings

void FillAttrLB::Fill( const XColorListRef &pColorTab )
{
    long nCount = pColorTab->Count();
    XColorEntry* pEntry;
    SetUpdateMode( false );

    for( long i = 0; i < nCount; i++ )
    {
        pEntry = pColorTab->GetColor( i );
        InsertEntry( pEntry->GetColor(), pEntry->GetName() );
    }

    AdaptDropDownLineCountToMaximum();
    SetUpdateMode( true );
}

// Fills the listbox (provisional) with strings

HatchingLB::HatchingLB( Window* pParent, WinBits nWinStyle)
: ListBox( pParent, nWinStyle ),
  mpList ( NULL )
{
    SetEdgeBlending(true);
}

extern "C" SAL_DLLPUBLIC_EXPORT Window* SAL_CALL makeHatchingLB(Window *pParent, VclBuilder::stringmap& rMap)
{
    WinBits nWinStyle = WB_LEFT|WB_VCENTER|WB_3DLOOK|WB_SIMPLEMODE;
    OString sBorder = VclBuilder::extractCustomProperty(rMap);
    if (!sBorder.isEmpty())
        nWinStyle |= WB_BORDER;
    HatchingLB *pListBox = new HatchingLB(pParent, nWinStyle);
    pListBox->EnableAutoSize(true);
    return pListBox;
}

void HatchingLB::Fill( const XHatchListRef &pList )
{
    if( !pList.is() )
        return;

    mpList = pList;
    XHatchEntry* pEntry;
    long nCount = pList->Count();

    SetUpdateMode( false );

    for( long i = 0; i < nCount; i++ )
    {
        pEntry = pList->GetHatch( i );
        const Bitmap aBitmap = pList->GetUiBitmap( i );
        if( !aBitmap.IsEmpty() )
            InsertEntry(pEntry->GetName(), Image(aBitmap));
        else
            InsertEntry( pEntry->GetName() );
    }

    AdaptDropDownLineCountToMaximum();
    SetUpdateMode( true );
}

/************************************************************************/

void HatchingLB::Append( const XHatchEntry& rEntry, const Bitmap& rBitmap )
{
    if(!rBitmap.IsEmpty())
    {
        InsertEntry(rEntry.GetName(), Image(rBitmap));
    }
    else
    {
        InsertEntry( rEntry.GetName() );
    }

    AdaptDropDownLineCountToMaximum();
}

/************************************************************************/

void HatchingLB::Modify( const XHatchEntry& rEntry, sal_Int32 nPos, const Bitmap& rBitmap )
{
    RemoveEntry( nPos );

    if( !rBitmap.IsEmpty() )
    {
        InsertEntry( rEntry.GetName(), Image(rBitmap), nPos );
    }
    else
    {
        InsertEntry( rEntry.GetName(), nPos );
    }
}

// Fills the listbox (provisional) with strings

void FillAttrLB::Fill( const XHatchListRef &pList )
{
    long nCount = pList->Count();
    XHatchEntry* pEntry;
    ListBox::SetUpdateMode( false );

    for( long i = 0; i < nCount; i++ )
    {
        pEntry = pList->GetHatch( i );
        const Bitmap aBitmap = pList->GetUiBitmap( i );
        if( !aBitmap.IsEmpty() )
            ListBox::InsertEntry(pEntry->GetName(), Image(aBitmap));
        else
            InsertEntry( pEntry->GetName() );
    }

    AdaptDropDownLineCountToMaximum();
    ListBox::SetUpdateMode( true );
}

// Fills the listbox (provisional) with strings

GradientLB::GradientLB( Window* pParent, WinBits aWB)
: ListBox( pParent, aWB ),
  mpList(NULL)
{
    SetEdgeBlending(true);
}

extern "C" SAL_DLLPUBLIC_EXPORT Window* SAL_CALL makeGradientLB(Window *pParent, VclBuilder::stringmap &rMap)
{
    WinBits nWinStyle = WB_LEFT|WB_VCENTER|WB_3DLOOK|WB_SIMPLEMODE;
    OString sBorder = VclBuilder::extractCustomProperty(rMap);
    if (!sBorder.isEmpty())
        nWinStyle |= WB_BORDER;
    GradientLB *pListBox = new GradientLB(pParent, nWinStyle);
    pListBox->EnableAutoSize(true);
    return pListBox;
}

void GradientLB::Fill( const XGradientListRef &pList )
{
    if( !pList.is() )
        return;

    mpList = pList;
    XGradientEntry* pEntry;
    long nCount = pList->Count();

    SetUpdateMode( false );

    for( long i = 0; i < nCount; i++ )
    {
        pEntry = pList->GetGradient( i );
        const Bitmap aBitmap = pList->GetUiBitmap( i );
        if( !aBitmap.IsEmpty() )
            InsertEntry(pEntry->GetName(), Image(aBitmap));
        else
            InsertEntry( pEntry->GetName() );
    }

    AdaptDropDownLineCountToMaximum();
    SetUpdateMode( true );
}

/************************************************************************/

void GradientLB::Append( const XGradientEntry& rEntry, const Bitmap& rBitmap )
{
    if(!rBitmap.IsEmpty())
    {
        InsertEntry(rEntry.GetName(), Image(rBitmap));
    }
    else
    {
        InsertEntry( rEntry.GetName() );
    }

    AdaptDropDownLineCountToMaximum();
}

/************************************************************************/

void GradientLB::Modify( const XGradientEntry& rEntry, sal_Int32 nPos, const Bitmap& rBitmap )
{
    RemoveEntry( nPos );

    if(!rBitmap.IsEmpty())
    {
        InsertEntry( rEntry.GetName(), Image(rBitmap), nPos );
    }
    else
    {
        InsertEntry( rEntry.GetName(), nPos );
    }
}

/************************************************************************/

void GradientLB::SelectEntryByList( const XGradientListRef &pList, const OUString& rStr,
                                    const XGradient& rGradient, sal_uInt16 nDist )
{
    long nCount = pList.get() ? pList->Count() : 0;
    XGradientEntry* pEntry;
    bool bFound = false;
    OUString aStr;

    long i;
    for( i = 0; i < nCount && !bFound; i++ )
    {
        pEntry = pList->GetGradient( i );

        aStr = pEntry->GetName();

        if( rStr == aStr && rGradient == pEntry->GetGradient() )
            bFound = true;
    }
    if( bFound )
        SelectEntryPos( (sal_uInt16) ( i - 1 + nDist ) );
}

// Fills the listbox (provisional) with strings

void FillAttrLB::Fill( const XGradientListRef &pList )
{
    long nCount = pList->Count();
    XGradientEntry* pEntry;
    ListBox::SetUpdateMode( false );

    for( long i = 0; i < nCount; i++ )
    {
        pEntry = pList->GetGradient( i );
        const Bitmap aBitmap = pList->GetUiBitmap( i );
        if( !aBitmap.IsEmpty() )
            ListBox::InsertEntry(pEntry->GetName(), Image(aBitmap));
        else
            InsertEntry( pEntry->GetName() );
    }

    AdaptDropDownLineCountToMaximum();
    ListBox::SetUpdateMode( true );
}

// BitmapLB Constructor

BitmapLB::BitmapLB( Window* pParent, WinBits aWB)
:   ListBox( pParent, aWB ),
    maBitmapEx(),
    mpList(NULL)
{
    SetEdgeBlending(true);
}

extern "C" SAL_DLLPUBLIC_EXPORT Window* SAL_CALL makeBitmapLB(Window *pParent, VclBuilder::stringmap &rMap)
{
    WinBits nWinStyle = WB_LEFT|WB_VCENTER|WB_3DLOOK|WB_SIMPLEMODE;
    OString sBorder = VclBuilder::extractCustomProperty(rMap);
    if (!sBorder.isEmpty())
        nWinStyle |= WB_BORDER;
    BitmapLB *pListBox = new BitmapLB(pParent, nWinStyle);
    pListBox->EnableAutoSize(true);
    return pListBox;
}

/************************************************************************/

namespace
{
    void formatBitmapExToSize(BitmapEx& rBitmapEx, const Size& rSize)
    {
        if(!rBitmapEx.IsEmpty() && rSize.Width() > 0 && rSize.Height() > 0)
        {
            VirtualDevice aVirtualDevice;
            aVirtualDevice.SetOutputSizePixel(rSize);

            if(rBitmapEx.IsTransparent())
            {
                const StyleSettings& rStyleSettings = Application::GetSettings().GetStyleSettings();

                if(rStyleSettings.GetPreviewUsesCheckeredBackground())
                {
                    const Point aNull(0, 0);
                    static const sal_uInt32 nLen(8);
                    static const Color aW(COL_WHITE);
                    static const Color aG(0xef, 0xef, 0xef);

                    aVirtualDevice.DrawCheckered(aNull, rSize, nLen, aW, aG);
                }
                else
                {
                    aVirtualDevice.SetBackground(rStyleSettings.GetFieldColor());
                    aVirtualDevice.Erase();
                }
            }

            if(rBitmapEx.GetSizePixel().Width() >= rSize.Width() && rBitmapEx.GetSizePixel().Height() >= rSize.Height())
            {
                rBitmapEx.Scale(rSize, BMP_SCALE_DEFAULT);
                aVirtualDevice.DrawBitmapEx(Point(0, 0), rBitmapEx);
            }
            else
            {
                const Size aBitmapSize(rBitmapEx.GetSizePixel());

                for(sal_Int32 y(0); y < rSize.Height(); y += aBitmapSize.Height())
                {
                    for(sal_Int32 x(0); x < rSize.Width(); x += aBitmapSize.Width())
                    {
                        aVirtualDevice.DrawBitmapEx(
                            Point(x, y),
                            rBitmapEx);
                    }
                }
            }

            rBitmapEx = aVirtualDevice.GetBitmap(Point(0, 0), rSize);
        }
    }
} // end of anonymous namespace

/************************************************************************/

void BitmapLB::Fill( const XBitmapListRef &pList )
{
    if( !pList.is() )
        return;

    mpList = pList;
    XBitmapEntry* pEntry;
    const long nCount(pList->Count());
    const StyleSettings& rStyleSettings = Application::GetSettings().GetStyleSettings();
    const Size aSize(rStyleSettings.GetListBoxPreviewDefaultPixelSize());

    SetUpdateMode(false);

    for(long i(0); i < nCount; i++)
    {
        pEntry = pList->GetBitmap(i);
        maBitmapEx = pEntry->GetGraphicObject().GetGraphic().GetBitmapEx();
        formatBitmapExToSize(maBitmapEx, aSize);
        InsertEntry(pEntry->GetName(), Image(maBitmapEx));
    }

    AdaptDropDownLineCountToMaximum();
    SetUpdateMode(true);
}

/************************************************************************/

void BitmapLB::Append(const Size& rSize, const XBitmapEntry& rEntry)
{
    maBitmapEx = rEntry.GetGraphicObject().GetGraphic().GetBitmapEx();

    if(!maBitmapEx.IsEmpty())
    {
        formatBitmapExToSize(maBitmapEx, rSize);
        InsertEntry(rEntry.GetName(), Image(maBitmapEx));
    }
    else
    {
        InsertEntry(rEntry.GetName());
    }

    AdaptDropDownLineCountToMaximum();
}

/************************************************************************/

void BitmapLB::Modify(const Size& rSize, const XBitmapEntry& rEntry, sal_Int32 nPos)
{
    RemoveEntry(nPos);
    maBitmapEx = rEntry.GetGraphicObject().GetGraphic().GetBitmapEx();

    if(!maBitmapEx.IsEmpty())
    {
        formatBitmapExToSize(maBitmapEx, rSize);
        InsertEntry(rEntry.GetName(), Image(maBitmapEx), nPos);
    }
    else
    {
        InsertEntry(rEntry.GetName());
    }
}

FillAttrLB::FillAttrLB(Window* pParent, WinBits aWB)
:   ColorListBox(pParent, aWB)
{
}

/************************************************************************/

void FillAttrLB::Fill( const XBitmapListRef &pList )
{
    const long nCount(pList->Count());
    XBitmapEntry* pEntry;
    const StyleSettings& rStyleSettings = Application::GetSettings().GetStyleSettings();
    const Size aSize(rStyleSettings.GetListBoxPreviewDefaultPixelSize());

    ListBox::SetUpdateMode(false);

    for(long i(0); i < nCount; i++)
    {
        pEntry = pList->GetBitmap( i );
        maBitmapEx = pEntry->GetGraphicObject().GetGraphic().GetBitmapEx();
        formatBitmapExToSize(maBitmapEx, aSize);
        ListBox::InsertEntry(pEntry->GetName(), Image(maBitmapEx));
    }

    AdaptDropDownLineCountToMaximum();
    ListBox::SetUpdateMode(true);
}

void FillTypeLB::Fill()
{
    SetUpdateMode( false );

    InsertEntry( SVX_RESSTR(RID_SVXSTR_INVISIBLE) );
    InsertEntry( SVX_RESSTR(RID_SVXSTR_COLOR) );
    InsertEntry( SVX_RESSTR(RID_SVXSTR_GRADIENT) );
    InsertEntry( SVX_RESSTR(RID_SVXSTR_HATCH) );
    InsertEntry( SVX_RESSTR(RID_SVXSTR_BITMAP) );

    AdaptDropDownLineCountToMaximum();
    SetUpdateMode( true );
}

LineLB::LineLB(Window* pParent, WinBits aWB)
:   ListBox(pParent, aWB),
    mbAddStandardFields(true)
{
    // No EdgeBlending for LineStyle/Dash SetEdgeBlending(true);
}

extern "C" SAL_DLLPUBLIC_EXPORT Window* SAL_CALL makeLineLB(Window *pParent, VclBuilder::stringmap &rMap)
{
    bool bDropdown = VclBuilder::extractDropdown(rMap);
    WinBits nWinBits = WB_LEFT|WB_VCENTER|WB_3DLOOK|WB_SIMPLEMODE|WB_TABSTOP;
    if (bDropdown)
        nWinBits |= WB_DROPDOWN;
    OString sBorder = VclBuilder::extractCustomProperty(rMap);
    if (!sBorder.isEmpty())
        nWinBits |= WB_BORDER;
    LineLB *pListBox = new LineLB(pParent, nWinBits);
    pListBox->EnableAutoSize(true);
    return pListBox;
}

LineLB::~LineLB()
{
}

void LineLB::setAddStandardFields(bool bNew)
{
    if(getAddStandardFields() != bNew)
    {
        mbAddStandardFields = bNew;
    }
}

// Fills the listbox (provisional) with strings

void LineLB::Fill( const XDashListRef &pList )
{
    Clear();

    if( !pList.is() )
        return;

    if(getAddStandardFields())
    {
        // entry for 'none'
        InsertEntry(pList->GetStringForUiNoLine());

        // entry for solid line
        InsertEntry(pList->GetStringForUiSolidLine(),
                Image(pList->GetBitmapForUISolidLine()));
    }

    // entries for dashed lines

    long nCount = pList->Count();
    XDashEntry* pEntry;
    SetUpdateMode( false );

    for( long i = 0; i < nCount; i++ )
    {
        pEntry = pList->GetDash( i );
        const Bitmap aBitmap = pList->GetUiBitmap( i );
        if( !aBitmap.IsEmpty() )
        {
            InsertEntry(pEntry->GetName(), Image(aBitmap));
        }
        else
            InsertEntry( pEntry->GetName() );
    }

    AdaptDropDownLineCountToMaximum();
    SetUpdateMode( true );
}

/************************************************************************/

void LineLB::Append( const XDashEntry& rEntry, const Bitmap& rBitmap )
{
    if(!rBitmap.IsEmpty())
    {
        InsertEntry(rEntry.GetName(), Image(rBitmap));
    }
    else
    {
        InsertEntry( rEntry.GetName() );
    }

    AdaptDropDownLineCountToMaximum();
}

/************************************************************************/

void LineLB::Modify( const XDashEntry& rEntry, sal_Int32 nPos, const Bitmap& rBitmap )
{
    RemoveEntry( nPos );

    if(!rBitmap.IsEmpty())
    {
        InsertEntry( rEntry.GetName(), Image(rBitmap), nPos );
    }
    else
    {
        InsertEntry( rEntry.GetName(), nPos );
    }
}

// Fills the listbox (provisional) with strings

LineEndLB::LineEndLB( Window* pParent, WinBits aWB )
    : ListBox( pParent, aWB )
{
    // No EdgeBlending for LineEnds SetEdgeBlending(true);
}

extern "C" SAL_DLLPUBLIC_EXPORT Window* SAL_CALL makeLineEndLB(Window *pParent, VclBuilder::stringmap &rMap)
{
    bool bDropdown = VclBuilder::extractDropdown(rMap);
    WinBits nWinBits = WB_LEFT|WB_VCENTER|WB_3DLOOK|WB_SIMPLEMODE|WB_TABSTOP;
    if (bDropdown)
        nWinBits |= WB_DROPDOWN;
    OString sBorder = VclBuilder::extractCustomProperty(rMap);
    if (!sBorder.isEmpty())
        nWinBits |= WB_BORDER;
    LineEndLB *pListBox = new LineEndLB(pParent, nWinBits);
    pListBox->EnableAutoSize(true);
    return pListBox;
}

LineEndLB::~LineEndLB(void)
{
}

void LineEndLB::Fill( const XLineEndListRef &pList, bool bStart )
{
    if( !pList.is() )
        return;

    long nCount = pList->Count();
    XLineEndEntry* pEntry;
    VirtualDevice aVD;
    SetUpdateMode( false );

    for( long i = 0; i < nCount; i++ )
    {
        pEntry = pList->GetLineEnd( i );
        const Bitmap aBitmap = pList->GetUiBitmap( i );
        if( !aBitmap.IsEmpty() )
        {
            Size aBmpSize( aBitmap.GetSizePixel() );
            aVD.SetOutputSizePixel( aBmpSize, false );
            aVD.DrawBitmap( Point(), aBitmap );
            InsertEntry( pEntry->GetName(),
                Image(aVD.GetBitmap(
                    (bStart) ? Point() : Point(aBmpSize.Width() / 2, 0),
                    Size(aBmpSize.Width() / 2, aBmpSize.Height()))));
            //delete pBitmap;
        }
        else
            InsertEntry( pEntry->GetName() );
    }

    AdaptDropDownLineCountToMaximum();
    SetUpdateMode( true );
}

/************************************************************************/

void LineEndLB::Append( const XLineEndEntry& rEntry, const Bitmap& rBitmap, bool bStart )
{
    if(!rBitmap.IsEmpty())
    {
        VirtualDevice aVD;
        const Size aBmpSize(rBitmap.GetSizePixel());

        aVD.SetOutputSizePixel(aBmpSize, false);
        aVD.DrawBitmap(Point(), rBitmap);
        InsertEntry(
            rEntry.GetName(),
            Image(aVD.GetBitmap(
                (bStart) ? Point() : Point(aBmpSize.Width() / 2, 0),
                Size(aBmpSize.Width() / 2, aBmpSize.Height()))));
    }
    else
    {
        InsertEntry(rEntry.GetName());
    }

    AdaptDropDownLineCountToMaximum();
}

/************************************************************************/

void LineEndLB::Modify( const XLineEndEntry& rEntry, sal_Int32 nPos, const Bitmap& rBitmap, bool bStart )
{
    RemoveEntry( nPos );

    if(!rBitmap.IsEmpty())
    {
        VirtualDevice aVD;
        const Size aBmpSize(rBitmap.GetSizePixel());

        aVD.SetOutputSizePixel(aBmpSize, false);
        aVD.DrawBitmap(Point(), rBitmap);
        InsertEntry(
            rEntry.GetName(),
            Image(aVD.GetBitmap(
                    (bStart) ? Point() : Point(aBmpSize.Width() / 2, 0),
                    Size(aBmpSize.Width() / 2, aBmpSize.Height()))),
            nPos);
    }
    else
    {
        InsertEntry(rEntry.GetName(), nPos);
    }
}



void SvxPreviewBase::InitSettings(bool bForeground, bool bBackground)
{
    const StyleSettings& rStyleSettings = Application::GetSettings().GetStyleSettings();

    if(bForeground)
    {
        svtools::ColorConfig aColorConfig;
        Color aTextColor(aColorConfig.GetColorValue(svtools::FONTCOLOR).nColor);

        if(IsControlForeground())
        {
            aTextColor = GetControlForeground();
        }

        getBufferDevice().SetTextColor(aTextColor);
    }

    if(bBackground)
    {
        if(IsControlBackground())
        {
            getBufferDevice().SetBackground(GetControlBackground());
        }
        else
        {
            getBufferDevice().SetBackground(rStyleSettings.GetWindowColor());
        }
    }

    // do not paint background self, it gets painted buffered
    SetControlBackground();
    SetBackground();

    Invalidate();
}

SvxPreviewBase::SvxPreviewBase(Window* pParent)
    : Control(pParent, WB_BORDER)
    , mpModel(new SdrModel())
    , mpBufferDevice(new VirtualDevice(*this))
{
    //  Draw the control's border as a flat thin black line.
    SetBorderStyle(WINDOW_BORDER_MONO);
    SetDrawMode( GetSettings().GetStyleSettings().GetHighContrastMode() ? OUTPUT_DRAWMODE_CONTRAST : OUTPUT_DRAWMODE_COLOR );
    SetMapMode(MAP_100TH_MM);

    // init model
    mpModel->GetItemPool().FreezeIdRanges();
}

SvxPreviewBase::~SvxPreviewBase()
{
    delete mpModel;
    delete mpBufferDevice;
}

void SvxPreviewBase::LocalPrePaint()
{
    // init BufferDevice
    if(mpBufferDevice->GetOutputSizePixel() != GetOutputSizePixel())
    {
        mpBufferDevice->SetDrawMode(GetDrawMode());
        mpBufferDevice->SetSettings(GetSettings());
        mpBufferDevice->SetAntialiasing(GetAntialiasing());
        mpBufferDevice->SetOutputSizePixel(GetOutputSizePixel());
        mpBufferDevice->SetMapMode(GetMapMode());
    }

    const StyleSettings& rStyleSettings = Application::GetSettings().GetStyleSettings();

    if(rStyleSettings.GetPreviewUsesCheckeredBackground())
    {
        const Point aNull(0, 0);
        static const sal_uInt32 nLen(8);
        static const Color aW(COL_WHITE);
        static const Color aG(0xef, 0xef, 0xef);
        const bool bWasEnabled(mpBufferDevice->IsMapModeEnabled());

        mpBufferDevice->EnableMapMode(false);
        mpBufferDevice->DrawCheckered(aNull, mpBufferDevice->GetOutputSizePixel(), nLen, aW, aG);
        mpBufferDevice->EnableMapMode(bWasEnabled);
    }
    else
    {
        mpBufferDevice->Erase();
    }
}

void SvxPreviewBase::LocalPostPaint()
{
    // copy to front (in pixel mode)
    const bool bWasEnabledSrc(mpBufferDevice->IsMapModeEnabled());
    const bool bWasEnabledDst(IsMapModeEnabled());
    const Point aEmptyPoint;

    mpBufferDevice->EnableMapMode(false);
    EnableMapMode(false);

    DrawOutDev(
        aEmptyPoint, GetOutputSizePixel(),
        aEmptyPoint, GetOutputSizePixel(),
        *mpBufferDevice);

    mpBufferDevice->EnableMapMode(bWasEnabledSrc);
    EnableMapMode(bWasEnabledDst);
}

void SvxPreviewBase::StateChanged(StateChangedType nType)
{
    Control::StateChanged(nType);

    if(STATE_CHANGE_CONTROLFOREGROUND == nType)
    {
        InitSettings(true, false);
    }
    else if(STATE_CHANGE_CONTROLBACKGROUND == nType)
    {
        InitSettings(false, true);
    }
}

void SvxPreviewBase::DataChanged(const DataChangedEvent& rDCEvt)
{
    SetDrawMode(GetSettings().GetStyleSettings().GetHighContrastMode() ? OUTPUT_DRAWMODE_CONTRAST : OUTPUT_DRAWMODE_COLOR);

    if((DATACHANGED_SETTINGS == rDCEvt.GetType()) && (rDCEvt.GetFlags() & SETTINGS_STYLE))
    {
        InitSettings(true, true);
    }
    else
    {
        Control::DataChanged(rDCEvt);
    }
}

void SvxXLinePreview::Resize()
{
    SvxPreviewBase::Resize();

    const Size aOutputSize(GetOutputSize());
    const sal_Int32 nDistance(500L);
    const sal_Int32 nAvailableLength(aOutputSize.Width() - (4 * nDistance));

    // create DrawObectA
    const sal_Int32 aYPosA(aOutputSize.Height() / 2);
    const basegfx::B2DPoint aPointA1( nDistance,  aYPosA);
    const basegfx::B2DPoint aPointA2( aPointA1.getX() + ((nAvailableLength * 14) / 20), aYPosA );
    basegfx::B2DPolygon aPolygonA;
    aPolygonA.append(aPointA1);
    aPolygonA.append(aPointA2);
    mpLineObjA->SetPathPoly(basegfx::B2DPolyPolygon(aPolygonA));

    // create DrawObectB
    const sal_Int32 aYPosB1((aOutputSize.Height() * 3) / 4);
    const sal_Int32 aYPosB2((aOutputSize.Height() * 1) / 4);
    const basegfx::B2DPoint aPointB1( aPointA2.getX() + nDistance,  aYPosB1);
    const basegfx::B2DPoint aPointB2( aPointB1.getX() + ((nAvailableLength * 2) / 20), aYPosB2 );
    const basegfx::B2DPoint aPointB3( aPointB2.getX() + ((nAvailableLength * 2) / 20), aYPosB1 );
    basegfx::B2DPolygon aPolygonB;
    aPolygonB.append(aPointB1);
    aPolygonB.append(aPointB2);
    aPolygonB.append(aPointB3);
    mpLineObjB->SetPathPoly(basegfx::B2DPolyPolygon(aPolygonB));

    // create DrawObectC
    basegfx::B2DPolygon aPolygonC;
    const basegfx::B2DPoint aPointC1( aPointB3.getX() + nDistance,  aYPosB1);
    const basegfx::B2DPoint aPointC2( aPointC1.getX() + ((nAvailableLength * 1) / 20), aYPosB2 );
    const basegfx::B2DPoint aPointC3( aPointC2.getX() + ((nAvailableLength * 1) / 20), aYPosB1 );
    aPolygonC.append(aPointC1);
    aPolygonC.append(aPointC2);
    aPolygonC.append(aPointC3);
    mpLineObjC->SetPathPoly(basegfx::B2DPolyPolygon(aPolygonC));
}

SvxXLinePreview::SvxXLinePreview(Window* pParent)
    : SvxPreviewBase(pParent)
    , mpLineObjA(NULL)
    , mpLineObjB(NULL)
    , mpLineObjC(NULL)
    , mpGraphic(NULL)
    , mbWithSymbol(false)
{
    InitSettings( true, true );

    mpLineObjA = new SdrPathObj(OBJ_LINE);
    mpLineObjA->SetModel(&getModel());

    mpLineObjB = new SdrPathObj(OBJ_PLIN);
    mpLineObjB->SetModel(&getModel());

    mpLineObjC = new SdrPathObj(OBJ_PLIN);
    mpLineObjC->SetModel(&getModel());
}

extern "C" SAL_DLLPUBLIC_EXPORT Window* SAL_CALL makeSvxXLinePreview(Window *pParent, VclBuilder::stringmap &)
{
    return new SvxXLinePreview(pParent);
}

Size SvxXLinePreview::GetOptimalSize() const
{
    return getPreviewStripSize(this);
}

SvxXLinePreview::~SvxXLinePreview()
{
    SdrObject *pFoo = mpLineObjA;
    SdrObject::Free( pFoo );
    pFoo = mpLineObjB;
    SdrObject::Free( pFoo );
    pFoo = mpLineObjC;
    SdrObject::Free( pFoo );
}



void SvxXLinePreview::SetSymbol(Graphic* p,const Size& s)
{
    mpGraphic = p;
    maSymbolSize = s;
}



void SvxXLinePreview::ResizeSymbol(const Size& s)
{
    if ( s != maSymbolSize )
    {
        maSymbolSize = s;
        Invalidate();
    }
}



void SvxXLinePreview::SetLineAttributes(const SfxItemSet& rItemSet)
{
    // Set ItemSet at objects
    mpLineObjA->SetMergedItemSet(rItemSet);

    // At line joints, do not use arrows
    SfxItemSet aTempSet(rItemSet);
    aTempSet.ClearItem(XATTR_LINESTART);
    aTempSet.ClearItem(XATTR_LINEEND);

    mpLineObjB->SetMergedItemSet(aTempSet);
    mpLineObjC->SetMergedItemSet(aTempSet);
}



void SvxXLinePreview::Paint( const Rectangle& )
{
    LocalPrePaint();

    // paint objects to buffer device
    sdr::contact::SdrObjectVector aObjectVector;
    aObjectVector.push_back(mpLineObjA);
    aObjectVector.push_back(mpLineObjB);
    aObjectVector.push_back(mpLineObjC);

    sdr::contact::ObjectContactOfObjListPainter aPainter(getBufferDevice(), aObjectVector, 0);
    sdr::contact::DisplayInfo aDisplayInfo;

    // do processing
    aPainter.ProcessDisplay(aDisplayInfo);

    if ( mbWithSymbol && mpGraphic )
    {
        const Size aOutputSize(GetOutputSize());
        Point aPos = Point( aOutputSize.Width() / 3, aOutputSize.Height() / 2 );
        aPos.X() -= maSymbolSize.Width() / 2;
        aPos.Y() -= maSymbolSize.Height() / 2;
        mpGraphic->Draw( &getBufferDevice(), aPos, maSymbolSize );
    }

    LocalPostPaint();
}

SvxXRectPreview::SvxXRectPreview(Window* pParent)
    : SvxPreviewBase(pParent)
    , mpRectangleObject(0)
{
    InitSettings(true, true);

    // create RectangleObject
    const Rectangle aObjectSize(Point(), GetOutputSize());
    mpRectangleObject = new SdrRectObj(aObjectSize);
    mpRectangleObject->SetModel(&getModel());
}

void SvxXRectPreview::Resize()
{
    const Rectangle aObjectSize(Point(), GetOutputSize());
    SdrObject *pOrigObject = mpRectangleObject;
    if (pOrigObject)
    {
        mpRectangleObject = new SdrRectObj(aObjectSize);
        mpRectangleObject->SetModel(&getModel());
        SetAttributes(pOrigObject->GetMergedItemSet());
        SdrObject::Free(pOrigObject);
    }
    SvxPreviewBase::Resize();
}

extern "C" SAL_DLLPUBLIC_EXPORT Window* SAL_CALL makeSvxXRectPreview(Window *pParent, VclBuilder::stringmap &)
{
    return new SvxXRectPreview(pParent);
}

SvxXRectPreview::~SvxXRectPreview()
{
    SdrObject::Free(mpRectangleObject);
}

void SvxXRectPreview::SetAttributes(const SfxItemSet& rItemSet)
{
    mpRectangleObject->SetMergedItemSet(rItemSet, true);
    mpRectangleObject->SetMergedItem(XLineStyleItem(XLINE_NONE));
}

void SvxXRectPreview::Paint( const Rectangle& )
{
    LocalPrePaint();

    sdr::contact::SdrObjectVector aObjectVector;

    aObjectVector.push_back(mpRectangleObject);

    sdr::contact::ObjectContactOfObjListPainter aPainter(getBufferDevice(), aObjectVector, 0);
    sdr::contact::DisplayInfo aDisplayInfo;

    aPainter.ProcessDisplay(aDisplayInfo);

    LocalPostPaint();
}

SvxXShadowPreview::SvxXShadowPreview( Window* pParent )
    : SvxPreviewBase(pParent)
    , mpRectangleObject(0)
    , mpRectangleShadow(0)
{
    InitSettings(true, true);

    // prepare size
    Size aSize = GetOutputSize();
    aSize.Width() = aSize.Width() / 3;
    aSize.Height() = aSize.Height() / 3;

    // create RectangleObject
    const Rectangle aObjectSize( Point( aSize.Width(), aSize.Height() ), aSize );
    mpRectangleObject = new SdrRectObj(aObjectSize);
    mpRectangleObject->SetModel(&getModel());

    // create ShadowObject
    const Rectangle aShadowSize( Point( aSize.Width(), aSize.Height() ), aSize );
    mpRectangleShadow = new SdrRectObj(aShadowSize);
    mpRectangleShadow->SetModel(&getModel());
}

extern "C" SAL_DLLPUBLIC_EXPORT Window* SAL_CALL makeSvxXShadowPreview (Window *pParent, VclBuilder::stringmap &)
{
    return new SvxXShadowPreview(pParent);
}

SvxXShadowPreview::~SvxXShadowPreview()
{
    SdrObject::Free(mpRectangleObject);
    SdrObject::Free(mpRectangleShadow);
}

void SvxXShadowPreview::SetRectangleAttributes(const SfxItemSet& rItemSet)
{
    mpRectangleObject->SetMergedItemSet(rItemSet, true);
    mpRectangleObject->SetMergedItem(XLineStyleItem(XLINE_NONE));
}

void SvxXShadowPreview::SetShadowAttributes(const SfxItemSet& rItemSet)
{
    mpRectangleShadow->SetMergedItemSet(rItemSet, true);
    mpRectangleShadow->SetMergedItem(XLineStyleItem(XLINE_NONE));
}

void SvxXShadowPreview::SetShadowPosition(const Point& rPos)
{
    maShadowOffset = rPos;
}

void SvxXShadowPreview::Paint( const Rectangle& )
{
    LocalPrePaint();

    // prepare size
    Size aSize = GetOutputSize();
    aSize.Width() = aSize.Width() / 3;
    aSize.Height() = aSize.Height() / 3;

    Rectangle aObjectRect(Point(aSize.Width(), aSize.Height()), aSize);
    mpRectangleObject->SetSnapRect(aObjectRect);
    aObjectRect.Move(maShadowOffset.X(), maShadowOffset.Y());
    mpRectangleShadow->SetSnapRect(aObjectRect);

    sdr::contact::SdrObjectVector aObjectVector;

    aObjectVector.push_back(mpRectangleShadow);
    aObjectVector.push_back(mpRectangleObject);

    sdr::contact::ObjectContactOfObjListPainter aPainter(getBufferDevice(), aObjectVector, 0);
    sdr::contact::DisplayInfo aDisplayInfo;

    aPainter.ProcessDisplay(aDisplayInfo);

    LocalPostPaint();
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
