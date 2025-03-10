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


#include <svx/tbxcolorupdate.hxx>
#include <svx/svxids.hrc>

#include <vcl/toolbox.hxx>
#include <vcl/bmpacc.hxx>
#include <vcl/settings.hxx>
#include <tools/debug.hxx>


namespace svx
{



    //= ToolboxButtonColorUpdater

    /* Note:
       The initial color shown on the button is set in /core/svx/source/tbxctrls/tbxcolorupdate.cxx
       (ToolboxButtonColorUpdater::ToolboxButtonColorUpdater()) .
       The initial color used by the button is set in /core/svx/source/tbxctrls/tbcontrl.cxx
       (SvxColorToolBoxControl::SvxColorToolBoxControl())
       and in case of writer for text(background)color also in /core/sw/source/uibase/docvw/edtwin.cxx
       (SwEditWin::m_aTextBackColor and SwEditWin::m_aTextColor)
     */

    ToolboxButtonColorUpdater::ToolboxButtonColorUpdater(
        sal_uInt16 nId,
        sal_uInt16 nTbxBtnId,
        ToolBox* ptrTbx) :
        mnBtnId           ( nTbxBtnId ),
        mnSlotId           ( nId ),
        mpTbx             ( ptrTbx ),
        maCurColor        ( COL_TRANSPARENT )
    {
        DBG_ASSERT( ptrTbx, "ToolBox not found :-(" );
        mbWasHiContrastMode = ptrTbx ? ( ptrTbx->GetSettings().GetStyleSettings().GetHighContrastMode() ) : sal_False;
        switch( mnSlotId )
        {
            case SID_ATTR_CHAR_COLOR  :
            case SID_ATTR_CHAR_COLOR2 :
                Update( COL_RED );
                break;
            case SID_FRAME_LINECOLOR  :
                Update( COL_BLUE );
                break;
            case SID_ATTR_CHAR_COLOR_BACKGROUND :
            case SID_BACKGROUND_COLOR :
                Update( COL_YELLOW );
                break;
            case SID_ATTR_LINE_COLOR:
                Update( COL_BLACK );
                break;
            default :
                Update( COL_TRANSPARENT );
        }
    }



    ToolboxButtonColorUpdater::~ToolboxButtonColorUpdater()
    {
    }



    void ToolboxButtonColorUpdater::Update( const Color& rColor )
    {
        Image       aImage( mpTbx->GetItemImage( mnBtnId ) );
        Size        aItemSize( mpTbx->GetItemContentSize( mnBtnId ) );

        const bool  bSizeChanged = ( maBmpSize != aItemSize );
        const bool  bDisplayModeChanged = ( mbWasHiContrastMode != mpTbx->GetSettings().GetStyleSettings().GetHighContrastMode() );
        Color       aColor( rColor );

        // !!! #109290# Workaround for SetFillColor with COL_AUTO
        if( aColor.GetColor() == COL_AUTO )
            aColor = Color( COL_TRANSPARENT );

        // For a shape selected in 'Draw', when color selected in Sidebar > Line > Color
        // is COL_BLACK, then (maCurColor != aColor) becomes 'false', therefore we take
        // explicit care of COL_BLACK from the last argument in the condition so that the
        // Update() does its routine job appropriately !
        if( ( maCurColor != aColor ) || bSizeChanged || bDisplayModeChanged || ( aColor == COL_BLACK ) )
        {
            // create an empty bitmap, and copy the original bitmap inside
            // (so that it grows in case the original bitmap was smaller)
            sal_uInt8 nAlpha = 255;
            BitmapEx aBmpEx( Bitmap( aItemSize, 24 ), AlphaMask( aItemSize, &nAlpha ) );
            BitmapEx aSource( aImage.GetBitmapEx() );
            Rectangle aRect( Point( 0, 0 ),
                    Size( std::min( aItemSize.Width(), aSource.GetSizePixel().Width() ), std::min( aItemSize.Height(), aSource.GetSizePixel().Height() ) ) );
            aBmpEx.CopyPixel( aRect, aRect, &aSource );

            Bitmap              aBmp( aBmpEx.GetBitmap() );
            BitmapWriteAccess*  pBmpAcc = aBmp.IsEmpty() ? NULL : aBmp.AcquireWriteAccess();

            maBmpSize = aBmp.GetSizePixel();

            if( pBmpAcc )
            {
                Bitmap              aMsk;
                BitmapWriteAccess*  pMskAcc;

                if( aBmpEx.IsAlpha() )
                    pMskAcc = ( aMsk = aBmpEx.GetAlpha().GetBitmap() ).AcquireWriteAccess();
                else if( aBmpEx.IsTransparent() )
                    pMskAcc = ( aMsk = aBmpEx.GetMask() ).AcquireWriteAccess();
                else
                    pMskAcc = NULL;

                mbWasHiContrastMode = mpTbx->GetSettings().GetStyleSettings().GetHighContrastMode();

                if( ( COL_TRANSPARENT != aColor.GetColor() ) && ( maBmpSize.Width() == maBmpSize.Height() ) )
                    pBmpAcc->SetLineColor( aColor );
                else if( mpTbx->GetBackground().GetColor().IsDark() )
                    pBmpAcc->SetLineColor( Color( COL_WHITE ) );
                else
                    pBmpAcc->SetLineColor( Color( COL_BLACK ) );

                // use not only COL_TRANSPARENT for detection of transparence,
                // but the method/way which is designed to do that
                const bool bIsTransparent(0xff == aColor.GetTransparency());
                maCurColor = aColor;

                if(bIsTransparent)
                {
                    pBmpAcc->SetFillColor();
                }
                else
                {
                    pBmpAcc->SetFillColor(maCurColor);
                }

                if( maBmpSize.Width() == maBmpSize.Height() )
                    maUpdRect = Rectangle( Point( 0, maBmpSize.Height() * 3 / 4 ), Size( maBmpSize.Width(), maBmpSize.Height() / 4 ) );
                else
                    maUpdRect = Rectangle( Point( maBmpSize.Height() + 2, 2 ), Point( maBmpSize.Width() - 3, maBmpSize.Height() - 3 ) );

                pBmpAcc->DrawRect( maUpdRect );

                if( pMskAcc )
                {
                    if( bIsTransparent )
                    {
                        pMskAcc->SetLineColor( COL_BLACK );
                        pMskAcc->SetFillColor( COL_WHITE );
                    }
                    else
                        pMskAcc->SetFillColor( COL_BLACK );

                    pMskAcc->DrawRect( maUpdRect );
                }

                aBmp.ReleaseAccess( pBmpAcc );

                if( pMskAcc )
                    aMsk.ReleaseAccess( pMskAcc );

                if( aBmpEx.IsAlpha() )
                    aBmpEx = BitmapEx( aBmp, AlphaMask( aMsk ) );
                else if( aBmpEx.IsTransparent() )
                    aBmpEx = BitmapEx( aBmp, aMsk );
                else
                    aBmpEx = aBmp;

                mpTbx->SetItemImage( mnBtnId, Image( aBmpEx ) );
            }
        }
    }


} // namespace svx


/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
