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


#include <toolkit/awt/vclxgraphics.hxx>
#include <toolkit/awt/vclxdevice.hxx>
#include <toolkit/awt/vclxfont.hxx>
#include <toolkit/helper/macros.hxx>
#include <toolkit/helper/vclunohelper.hxx>
#include <cppuhelper/typeprovider.hxx>
#include <rtl/uuid.h>

#include <vcl/svapp.hxx>
#include <vcl/outdev.hxx>
#include <vcl/image.hxx>
#include <vcl/gradient.hxx>
#include <tools/debug.hxx>

using namespace com::sun::star;


//  class VCLXGraphics


// uno::XInterface
uno::Any VCLXGraphics::queryInterface( const uno::Type & rType ) throw(uno::RuntimeException, std::exception)
{
    uno::Any aRet = ::cppu::queryInterface( rType,
                                        (static_cast< ::com::sun::star::awt::XGraphics* >(this)),
                                        (static_cast< ::com::sun::star::lang::XTypeProvider* >(this)),
                                        (static_cast< ::com::sun::star::lang::XUnoTunnel* >(this)) );
    return (aRet.hasValue() ? aRet : OWeakObject::queryInterface( rType ));
}

// lang::XUnoTunnel
IMPL_XUNOTUNNEL( VCLXGraphics )

// lang::XTypeProvider
IMPL_XTYPEPROVIDER_START( VCLXGraphics )
    cppu::UnoType<awt::XGraphics>::get()
IMPL_XTYPEPROVIDER_END

VCLXGraphics::VCLXGraphics()
    : mpOutputDevice(NULL)
    , meRasterOp(ROP_OVERPAINT)
    , mpClipRegion(NULL)
{
}

VCLXGraphics::~VCLXGraphics()
{
    VCLXGraphicsList_impl* pLst = mpOutputDevice ? mpOutputDevice->GetUnoGraphicsList() : NULL;
    if ( pLst )
    {
        for( VCLXGraphicsList_impl::iterator it = pLst->begin(); it != pLst->end(); ++it )
        {
            if( *it == this ) {
                pLst->erase( it );
                break;
            }
        }
    }

    delete mpClipRegion;
}

void VCLXGraphics::SetOutputDevice( OutputDevice* pOutDev )
{
    mpOutputDevice = pOutDev;
    mxDevice = NULL;
    initAttrs();
}

void VCLXGraphics::Init( OutputDevice* pOutDev )
{
    DBG_ASSERT( !mpOutputDevice, "VCLXGraphics::Init already has pOutDev !" );
    mpOutputDevice  = pOutDev;

    initAttrs();
    mpClipRegion    = NULL;

    // Register at OutputDevice
    VCLXGraphicsList_impl* pLst = mpOutputDevice->GetUnoGraphicsList();
    if ( !pLst )
        pLst = mpOutputDevice->CreateUnoGraphicsList();
    pLst->push_back( this );
}

void VCLXGraphics::initAttrs()
{
    if ( !mpOutputDevice )
        return;

    maFont          = mpOutputDevice->GetFont();
    maTextColor     = mpOutputDevice->GetTextColor(); /* COL_BLACK */
    maTextFillColor = mpOutputDevice->GetTextFillColor(); /* COL_TRANSPARENT */
    maLineColor     = mpOutputDevice->GetLineColor(); /* COL_BLACK */
    maFillColor     = mpOutputDevice->GetFillColor(); /* COL_WHITE */
    meRasterOp      = mpOutputDevice->GetRasterOp(); /* ROP_OVERPAINT */
}

void VCLXGraphics::InitOutputDevice( sal_uInt16 nFlags )
{
    if(mpOutputDevice)
    {
        SolarMutexGuard aVclGuard;

        if ( nFlags & INITOUTDEV_FONT )
        {
            mpOutputDevice->SetFont( maFont );
            mpOutputDevice->SetTextColor( maTextColor );
            mpOutputDevice->SetTextFillColor( maTextFillColor );
        }

        if ( nFlags & INITOUTDEV_COLORS )
        {
            mpOutputDevice->SetLineColor( maLineColor );
            mpOutputDevice->SetFillColor( maFillColor );
        }

        if ( nFlags & INITOUTDEV_RASTEROP )
        {
            mpOutputDevice->SetRasterOp( meRasterOp );
        }

        if ( nFlags & INITOUTDEV_CLIPREGION )
        {
            if( mpClipRegion )
                mpOutputDevice->SetClipRegion( *mpClipRegion );
            else
                mpOutputDevice->SetClipRegion();
        }
    }
}

uno::Reference< awt::XDevice > VCLXGraphics::getDevice() throw(uno::RuntimeException, std::exception)
{
    SolarMutexGuard aGuard;

    if( !mxDevice.is() && mpOutputDevice )
    {
        VCLXDevice* pDev = new VCLXDevice;
        pDev->SetOutputDevice( mpOutputDevice );
        mxDevice = pDev;
    }
    return mxDevice;
}

awt::SimpleFontMetric VCLXGraphics::getFontMetric() throw(uno::RuntimeException, std::exception)
{
    SolarMutexGuard aGuard;

    awt::SimpleFontMetric aM;
    if( mpOutputDevice )
    {
        mpOutputDevice->SetFont( maFont );
        aM = VCLUnoHelper::CreateFontMetric( mpOutputDevice->GetFontMetric() );
    }
    return aM;
}

void VCLXGraphics::setFont( const uno::Reference< awt::XFont >& rxFont ) throw(uno::RuntimeException, std::exception)
{
    SolarMutexGuard aGuard;

    maFont = VCLUnoHelper::CreateFont( rxFont );
}

uno::Reference< awt::XFont > VCLXGraphics::getFont() throw(uno::RuntimeException)
{
    uno::Reference< awt::XFont > xFont;
    uno::Reference< awt::XDevice > xDevice( getDevice() );

    SolarMutexGuard aGuard;

    if ( xDevice.is() )
    {
        VCLXFont *pFont = new VCLXFont;
        pFont->Init( *xDevice.get(), maFont );
        xFont.set( static_cast< ::cppu::OWeakObject* >( pFont ), uno::UNO_QUERY );
    }

    return xFont;
}

void VCLXGraphics::selectFont( const awt::FontDescriptor& rDescription ) throw(uno::RuntimeException, std::exception)
{
    SolarMutexGuard aGuard;

    maFont = VCLUnoHelper::CreateFont( rDescription, Font() );
}

void VCLXGraphics::setTextColor( sal_Int32 nColor ) throw(uno::RuntimeException, std::exception)
{
    SolarMutexGuard aGuard;

    maTextColor = Color( (sal_uInt32)nColor );
}

::sal_Int32 VCLXGraphics::getTextColor() throw(uno::RuntimeException)
{
    SolarMutexGuard aGuard;
    return maTextColor.GetColor();
}

void VCLXGraphics::setTextFillColor( sal_Int32 nColor ) throw(uno::RuntimeException, std::exception)
{
    SolarMutexGuard aGuard;

    maTextFillColor = Color( (sal_uInt32)nColor );
}

::sal_Int32 VCLXGraphics::getTextFillColor() throw(uno::RuntimeException)
{
    SolarMutexGuard aGuard;
    return maTextFillColor.GetColor();
}

void VCLXGraphics::setLineColor( sal_Int32 nColor ) throw(uno::RuntimeException, std::exception)
{
    SolarMutexGuard aGuard;

    maLineColor = Color( (sal_uInt32)nColor );
}

::sal_Int32 VCLXGraphics::getLineColor() throw(uno::RuntimeException)
{
    SolarMutexGuard aGuard;
    return maLineColor.GetColor();
}

void VCLXGraphics::setFillColor( sal_Int32 nColor ) throw(uno::RuntimeException, std::exception)
{
    SolarMutexGuard aGuard;

    maFillColor = Color( (sal_uInt32)nColor );
}

::sal_Int32 VCLXGraphics::getFillColor() throw(uno::RuntimeException)
{
    SolarMutexGuard aGuard;
    return maFillColor.GetColor();
}

void VCLXGraphics::setRasterOp( awt::RasterOperation eROP ) throw(uno::RuntimeException, std::exception)
{
    SolarMutexGuard aGuard;

    meRasterOp = (RasterOp)eROP;
}

awt::RasterOperation VCLXGraphics::getRasterOp()
throw(uno::RuntimeException)
{
    SolarMutexGuard aGuard;
    return (awt::RasterOperation) meRasterOp;
}

void VCLXGraphics::setClipRegion( const uno::Reference< awt::XRegion >& rxRegion ) throw(uno::RuntimeException, std::exception)
{
    SolarMutexGuard aGuard;

    delete mpClipRegion;
    if ( rxRegion.is() )
        mpClipRegion = new Region( VCLUnoHelper::GetRegion( rxRegion ) );
    else
        mpClipRegion = NULL;
}

void VCLXGraphics::intersectClipRegion( const uno::Reference< awt::XRegion >& rxRegion ) throw(uno::RuntimeException, std::exception)
{
    SolarMutexGuard aGuard;

    if ( rxRegion.is() )
    {
        Region aRegion( VCLUnoHelper::GetRegion( rxRegion ) );
        if ( !mpClipRegion )
            mpClipRegion = new Region( aRegion );
        else
            mpClipRegion->Intersect( aRegion );
    }
}

void VCLXGraphics::push(  ) throw(uno::RuntimeException, std::exception)
{
    SolarMutexGuard aGuard;


    if( mpOutputDevice )
        mpOutputDevice->Push();
}

void VCLXGraphics::pop(  ) throw(uno::RuntimeException, std::exception)
{
    SolarMutexGuard aGuard;


    if( mpOutputDevice )
        mpOutputDevice->Pop();
}

void VCLXGraphics::clear(
    const awt::Rectangle& aRect )
throw(uno::RuntimeException, std::exception)
{
    SolarMutexGuard aGuard;

    if( mpOutputDevice )
    {
        const ::Rectangle aVCLRect = VCLUnoHelper::ConvertToVCLRect( aRect );
        mpOutputDevice->Erase( aVCLRect );
    }
}

void VCLXGraphics::copy( const uno::Reference< awt::XDevice >& rxSource, sal_Int32 nSourceX, sal_Int32 nSourceY, sal_Int32 nSourceWidth, sal_Int32 nSourceHeight, sal_Int32 nDestX, sal_Int32 nDestY, sal_Int32 nDestWidth, sal_Int32 nDestHeight ) throw(uno::RuntimeException, std::exception)
{
    SolarMutexGuard aGuard;

    if ( mpOutputDevice )
    {
        VCLXDevice* pFromDev = VCLXDevice::GetImplementation( rxSource );
        DBG_ASSERT( pFromDev, "VCLXGraphics::copy - invalid device" );
        if ( pFromDev )
        {
            InitOutputDevice( INITOUTDEV_CLIPREGION|INITOUTDEV_RASTEROP );
            mpOutputDevice->DrawOutDev( Point( nDestX, nDestY ), Size( nDestWidth, nDestHeight ),
                                    Point( nSourceX, nSourceY ), Size( nSourceWidth, nSourceHeight ), *pFromDev->GetOutputDevice() );
        }
    }
}

void VCLXGraphics::draw( const uno::Reference< awt::XDisplayBitmap >& rxBitmapHandle, sal_Int32 nSourceX, sal_Int32 nSourceY, sal_Int32 nSourceWidth, sal_Int32 nSourceHeight, sal_Int32 nDestX, sal_Int32 nDestY, sal_Int32 nDestWidth, sal_Int32 nDestHeight ) throw(uno::RuntimeException, std::exception)
{
    SolarMutexGuard aGuard;

    if( mpOutputDevice )
    {
        InitOutputDevice( INITOUTDEV_CLIPREGION|INITOUTDEV_RASTEROP);
        uno::Reference< awt::XBitmap > xBitmap( rxBitmapHandle, uno::UNO_QUERY );
        BitmapEx aBmpEx = VCLUnoHelper::GetBitmap( xBitmap );

        Point aPos(nDestX - nSourceX, nDestY - nSourceY);
          Size aSz = aBmpEx.GetSizePixel();

        if(nDestWidth != nSourceWidth)
        {
            float zoomX = (float)nDestWidth / (float)nSourceWidth;
            aSz.Width() = (long) ((float)aSz.Width() * zoomX);
        }

        if(nDestHeight != nSourceHeight)
        {
            float zoomY = (float)nDestHeight / (float)nSourceHeight;
            aSz.Height() = (long) ((float)aSz.Height() * zoomY);
        }

        if(nSourceX || nSourceY || aSz.Width() != nSourceWidth || aSz.Height() != nSourceHeight)
            mpOutputDevice->IntersectClipRegion(Region(Rectangle(nDestX, nDestY, nDestX + nDestWidth - 1, nDestY + nDestHeight - 1)));

        mpOutputDevice->DrawBitmapEx( aPos, aSz, aBmpEx );
    }
}

void VCLXGraphics::drawPixel( sal_Int32 x, sal_Int32 y ) throw(uno::RuntimeException, std::exception)
{
    SolarMutexGuard aGuard;

    if( mpOutputDevice )
    {
        InitOutputDevice( INITOUTDEV_CLIPREGION|INITOUTDEV_RASTEROP|INITOUTDEV_COLORS );
        mpOutputDevice->DrawPixel( Point( x, y ) );
    }
}

void VCLXGraphics::drawLine( sal_Int32 x1, sal_Int32 y1, sal_Int32 x2, sal_Int32 y2 ) throw(uno::RuntimeException, std::exception)
{
    SolarMutexGuard aGuard;

    if( mpOutputDevice )
    {
        InitOutputDevice( INITOUTDEV_CLIPREGION|INITOUTDEV_RASTEROP|INITOUTDEV_COLORS );
        mpOutputDevice->DrawLine( Point( x1, y1 ), Point( x2, y2 ) );
    }
}

void VCLXGraphics::drawRect( sal_Int32 x, sal_Int32 y, sal_Int32 width, sal_Int32 height ) throw(uno::RuntimeException, std::exception)
{
    SolarMutexGuard aGuard;

    if( mpOutputDevice )
    {
        InitOutputDevice( INITOUTDEV_CLIPREGION|INITOUTDEV_RASTEROP|INITOUTDEV_COLORS );
        mpOutputDevice->DrawRect( Rectangle( Point( x, y ), Size( width, height ) ) );
    }
}

void VCLXGraphics::drawRoundedRect( sal_Int32 x, sal_Int32 y, sal_Int32 width, sal_Int32 height, sal_Int32 nHorzRound, sal_Int32 nVertRound ) throw(uno::RuntimeException, std::exception)
{
    SolarMutexGuard aGuard;

    if( mpOutputDevice )
    {
        InitOutputDevice( INITOUTDEV_CLIPREGION|INITOUTDEV_RASTEROP|INITOUTDEV_COLORS );
        mpOutputDevice->DrawRect( Rectangle( Point( x, y ), Size( width, height ) ), nHorzRound, nVertRound );
    }
}

void VCLXGraphics::drawPolyLine( const uno::Sequence< sal_Int32 >& DataX, const uno::Sequence< sal_Int32 >& DataY ) throw(uno::RuntimeException, std::exception)
{
    SolarMutexGuard aGuard;

    if( mpOutputDevice )
    {
        InitOutputDevice( INITOUTDEV_CLIPREGION|INITOUTDEV_RASTEROP|INITOUTDEV_COLORS );
        mpOutputDevice->DrawPolyLine( VCLUnoHelper::CreatePolygon( DataX, DataY ) );
    }
}

void VCLXGraphics::drawPolygon( const uno::Sequence< sal_Int32 >& DataX, const uno::Sequence< sal_Int32 >& DataY ) throw(uno::RuntimeException, std::exception)
{
    SolarMutexGuard aGuard;

    if( mpOutputDevice )
    {
        InitOutputDevice( INITOUTDEV_CLIPREGION|INITOUTDEV_RASTEROP|INITOUTDEV_COLORS );
        mpOutputDevice->DrawPolygon( VCLUnoHelper::CreatePolygon( DataX, DataY ) );
    }
}

void VCLXGraphics::drawPolyPolygon( const uno::Sequence< uno::Sequence< sal_Int32 > >& DataX, const uno::Sequence< uno::Sequence< sal_Int32 > >& DataY ) throw(uno::RuntimeException, std::exception)
{
    SolarMutexGuard aGuard;

    if( mpOutputDevice )
    {
        InitOutputDevice( INITOUTDEV_CLIPREGION|INITOUTDEV_RASTEROP|INITOUTDEV_COLORS );
        sal_uInt16 nPolys = (sal_uInt16) DataX.getLength();
        PolyPolygon aPolyPoly( nPolys );
        for ( sal_uInt16 n = 0; n < nPolys; n++ )
            aPolyPoly[n] = VCLUnoHelper::CreatePolygon( DataX.getConstArray()[n], DataY.getConstArray()[n] );

        mpOutputDevice->DrawPolyPolygon( aPolyPoly );
    }
}

void VCLXGraphics::drawEllipse( sal_Int32 x, sal_Int32 y, sal_Int32 width, sal_Int32 height ) throw(uno::RuntimeException, std::exception)
{
    SolarMutexGuard aGuard;

    if( mpOutputDevice )
    {
        InitOutputDevice( INITOUTDEV_CLIPREGION|INITOUTDEV_RASTEROP|INITOUTDEV_COLORS );
        mpOutputDevice->DrawEllipse( Rectangle( Point( x, y ), Size( width, height ) ) );
    }
}

void VCLXGraphics::drawArc( sal_Int32 x, sal_Int32 y, sal_Int32 width, sal_Int32 height, sal_Int32 x1, sal_Int32 y1, sal_Int32 x2, sal_Int32 y2 ) throw(uno::RuntimeException, std::exception)
{
    SolarMutexGuard aGuard;

    if( mpOutputDevice )
    {
        InitOutputDevice( INITOUTDEV_CLIPREGION|INITOUTDEV_RASTEROP|INITOUTDEV_COLORS );
        mpOutputDevice->DrawArc( Rectangle( Point( x, y ), Size( width, height ) ), Point( x1, y1 ), Point( x2, y2 ) );
    }
}

void VCLXGraphics::drawPie( sal_Int32 x, sal_Int32 y, sal_Int32 width, sal_Int32 height, sal_Int32 x1, sal_Int32 y1, sal_Int32 x2, sal_Int32 y2 ) throw(uno::RuntimeException, std::exception)
{
    SolarMutexGuard aGuard;

    if( mpOutputDevice )
    {
        InitOutputDevice( INITOUTDEV_CLIPREGION|INITOUTDEV_RASTEROP|INITOUTDEV_COLORS );
        mpOutputDevice->DrawPie( Rectangle( Point( x, y ), Size( width, height ) ), Point( x1, y1 ), Point( x2, y2 ) );
    }
}

void VCLXGraphics::drawChord( sal_Int32 x, sal_Int32 y, sal_Int32 width, sal_Int32 height, sal_Int32 x1, sal_Int32 y1, sal_Int32 x2, sal_Int32 y2 ) throw(uno::RuntimeException, std::exception)
{
    SolarMutexGuard aGuard;

    if( mpOutputDevice )
    {
        InitOutputDevice( INITOUTDEV_CLIPREGION|INITOUTDEV_RASTEROP|INITOUTDEV_COLORS );
        mpOutputDevice->DrawChord( Rectangle( Point( x, y ), Size( width, height ) ), Point( x1, y1 ), Point( x2, y2 ) );
    }
}

void VCLXGraphics::drawGradient( sal_Int32 x, sal_Int32 y, sal_Int32 width, sal_Int32 height, const awt::Gradient& rGradient ) throw(uno::RuntimeException, std::exception)
{
    SolarMutexGuard aGuard;

    if( mpOutputDevice )
    {
        InitOutputDevice( INITOUTDEV_CLIPREGION|INITOUTDEV_RASTEROP|INITOUTDEV_COLORS );
        Gradient aGradient((GradientStyle)rGradient.Style, rGradient.StartColor, rGradient.EndColor);
        aGradient.SetAngle(rGradient.Angle);
        aGradient.SetBorder(rGradient.Border);
        aGradient.SetOfsX(rGradient.XOffset);
        aGradient.SetOfsY(rGradient.YOffset);
        aGradient.SetStartIntensity(rGradient.StartIntensity);
        aGradient.SetEndIntensity(rGradient.EndIntensity);
        aGradient.SetSteps(rGradient.StepCount);
        mpOutputDevice->DrawGradient( Rectangle( Point( x, y ), Size( width, height ) ), aGradient );
    }
}

void VCLXGraphics::drawText( sal_Int32 x, sal_Int32 y, const OUString& rText ) throw(uno::RuntimeException, std::exception)
{
    SolarMutexGuard aGuard;

    if( mpOutputDevice )
    {
        InitOutputDevice( INITOUTDEV_CLIPREGION|INITOUTDEV_RASTEROP|INITOUTDEV_COLORS |INITOUTDEV_FONT);
        mpOutputDevice->DrawText( Point( x, y ), rText );
    }
}

void VCLXGraphics::drawTextArray( sal_Int32 x, sal_Int32 y, const OUString& rText, const uno::Sequence< sal_Int32 >& rLongs ) throw(uno::RuntimeException, std::exception)
{
    SolarMutexGuard aGuard;

    if( mpOutputDevice )
    {
        InitOutputDevice( INITOUTDEV_CLIPREGION|INITOUTDEV_RASTEROP|INITOUTDEV_COLORS|INITOUTDEV_FONT );
        long* pDXA = (long*)alloca(rText.getLength() * sizeof(long));
        for(int i = 0; i < rText.getLength(); i++)
        {
            pDXA[i] = rLongs[i];
        }
        mpOutputDevice->DrawTextArray( Point( x, y ), rText, pDXA );
    }
}


void VCLXGraphics::drawImage( sal_Int32 x, sal_Int32 y, sal_Int32 width, sal_Int32 height, sal_Int16 nStyle, const uno::Reference< graphic::XGraphic >& xGraphic ) throw(uno::RuntimeException, std::exception)
{
    SolarMutexGuard aGuard;

    if( mpOutputDevice && xGraphic.is() )
    {
        Image aImage( xGraphic );
        if ( !!aImage )
        {
            InitOutputDevice( INITOUTDEV_CLIPREGION|INITOUTDEV_RASTEROP|INITOUTDEV_COLORS );
            mpOutputDevice->DrawImage( Point( x, y ), Size( width, height ), aImage, nStyle );
        }
    }
}
/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
