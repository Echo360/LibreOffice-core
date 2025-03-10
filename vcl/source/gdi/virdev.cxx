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

#include <tools/debug.hxx>

#include <vcl/settings.hxx>
#include <vcl/svapp.hxx>
#include <vcl/wrkwin.hxx>
#include <vcl/virdev.hxx>

#include <salinst.hxx>
#include <salgdi.hxx>
#include <salframe.hxx>
#include <salvd.hxx>
#include <outdev.h>
#include "PhysicalFontCollection.hxx"
#include <svdata.hxx>

#include <vcl/ITiledRenderable.hxx>

namespace vcl
{

ITiledRenderable::~ITiledRenderable()
{
}

}

using namespace ::com::sun::star::uno;

bool VirtualDevice::AcquireGraphics() const
{
    DBG_TESTSOLARMUTEX();

    if ( mpGraphics )
        return true;

    mbInitLineColor     = true;
    mbInitFillColor     = true;
    mbInitFont          = true;
    mbInitTextColor     = true;
    mbInitClipRegion    = true;

    ImplSVData* pSVData = ImplGetSVData();

    if ( mpVirDev )
    {
        mpGraphics = mpVirDev->AcquireGraphics();
        // if needed retry after releasing least recently used virtual device graphics
        while ( !mpGraphics )
        {
            if ( !pSVData->maGDIData.mpLastVirGraphics )
                break;
            pSVData->maGDIData.mpLastVirGraphics->ReleaseGraphics();
            mpGraphics = mpVirDev->AcquireGraphics();
        }
        // update global LRU list of virtual device graphics
        if ( mpGraphics )
        {
            mpNextGraphics = pSVData->maGDIData.mpFirstVirGraphics;
            pSVData->maGDIData.mpFirstVirGraphics = const_cast<VirtualDevice*>(this);
            if ( mpNextGraphics )
                mpNextGraphics->mpPrevGraphics = const_cast<VirtualDevice*>(this);
            if ( !pSVData->maGDIData.mpLastVirGraphics )
                pSVData->maGDIData.mpLastVirGraphics = const_cast<VirtualDevice*>(this);
        }
    }

    if ( mpGraphics )
    {
        mpGraphics->SetXORMode( (ROP_INVERT == meRasterOp) || (ROP_XOR == meRasterOp), ROP_INVERT == meRasterOp );
        mpGraphics->setAntiAliasB2DDraw(mnAntialiasing & ANTIALIASING_ENABLE_B2DDRAW);
    }

    return mpGraphics ? true : false;
}

void VirtualDevice::ReleaseGraphics( bool bRelease )
{
    DBG_TESTSOLARMUTEX();

    if ( !mpGraphics )
        return;

    // release the fonts of the physically released graphics device
    if ( bRelease )
        ImplReleaseFonts();

    ImplSVData* pSVData = ImplGetSVData();

    VirtualDevice* pVirDev = (VirtualDevice*)this;

    if ( bRelease )
        pVirDev->mpVirDev->ReleaseGraphics( mpGraphics );
    // remove from global LRU list of virtual device graphics
    if ( mpPrevGraphics )
        mpPrevGraphics->mpNextGraphics = mpNextGraphics;
    else
        pSVData->maGDIData.mpFirstVirGraphics = mpNextGraphics;
    if ( mpNextGraphics )
        mpNextGraphics->mpPrevGraphics = mpPrevGraphics;
    else
        pSVData->maGDIData.mpLastVirGraphics = mpPrevGraphics;

    mpGraphics      = NULL;
    mpPrevGraphics  = NULL;
    mpNextGraphics  = NULL;
}

void VirtualDevice::ImplInitVirDev( const OutputDevice* pOutDev,
                                    long nDX, long nDY, sal_uInt16 nBitCount, const SystemGraphicsData *pData )
{
    SAL_INFO( "vcl.virdev", "ImplInitVirDev(" << nDX << "," << nDY << "," << nBitCount << ")" );

    if ( nDX < 1 )
        nDX = 1;

    if ( nDY < 1 )
        nDY = 1;

    ImplSVData* pSVData = ImplGetSVData();

    if ( !pOutDev )
        pOutDev = ImplGetDefaultWindow();
    if( !pOutDev )
        return;

    SalGraphics* pGraphics;
    if ( !pOutDev->mpGraphics )
        pOutDev->AcquireGraphics();
    pGraphics = pOutDev->mpGraphics;
    if ( pGraphics )
        mpVirDev = pSVData->mpDefInst->CreateVirtualDevice( pGraphics, nDX, nDY, nBitCount, pData );
    else
        mpVirDev = NULL;
    if ( !mpVirDev )
    {
        // do not abort but throw an exception, may be the current thread terminates anyway (plugin-scenario)
        throw ::com::sun::star::uno::RuntimeException(
            OUString( "Could not create system bitmap!" ),
            ::com::sun::star::uno::Reference< ::com::sun::star::uno::XInterface >() );
    }

    mnBitCount      = ( nBitCount ? nBitCount : pOutDev->GetBitCount() );
    mnOutWidth      = nDX;
    mnOutHeight     = nDY;
    mbScreenComp    = true;
    mnAlphaDepth    = -1;

    // #i59315# init vdev size from system object, when passed a
    // SystemGraphicsData. Otherwise, output size will always
    // incorrectly stay at (1,1)
    if( pData && mpVirDev )
        mpVirDev->GetSize(mnOutWidth,mnOutHeight);

    if( mnBitCount < 8 )
        SetAntialiasing( ANTIALIASING_DISABLE_TEXT );

    if ( pOutDev->GetOutDevType() == OUTDEV_PRINTER )
        mbScreenComp = false;
    else if ( pOutDev->GetOutDevType() == OUTDEV_VIRDEV )
        mbScreenComp = ((VirtualDevice*)pOutDev)->mbScreenComp;

    meOutDevType    = OUTDEV_VIRDEV;
    mbDevOutput     = true;
    mpFontCollection      = pSVData->maGDIData.mpScreenFontList;
    mpFontCache     = pSVData->maGDIData.mpScreenFontCache;
    mnDPIX          = pOutDev->mnDPIX;
    mnDPIY          = pOutDev->mnDPIY;
    mnDPIScaleFactor = pOutDev->mnDPIScaleFactor;
    maFont          = pOutDev->maFont;

    if( maTextColor != pOutDev->maTextColor )
    {
        maTextColor = pOutDev->maTextColor;
        mbInitTextColor = true;
    }

    // virtual devices have white background by default
    SetBackground( Wallpaper( Color( COL_WHITE ) ) );

    // #i59283# don't erase user-provided surface
    if( !pData )
        Erase();

    // register VirDev in the list
    mpNext = pSVData->maGDIData.mpFirstVirDev;
    mpPrev = NULL;
    if ( mpNext )
        mpNext->mpPrev = this;
    else
        pSVData->maGDIData.mpLastVirDev = this;
    pSVData->maGDIData.mpFirstVirDev = this;
}

VirtualDevice::VirtualDevice( sal_uInt16 nBitCount )
:   mpVirDev( NULL ),
    meRefDevMode( REFDEV_NONE )
{
    SAL_WARN_IF( nBitCount > 1 && nBitCount != 8, "vcl.gdi",
                 "VirtualDevice::VirtualDevice(): Only 0, 1 or 8 allowed for BitCount, not " << nBitCount );
    SAL_INFO( "vcl.gdi", "VirtualDevice::VirtualDevice( " << nBitCount << " )" );

    ImplInitVirDev( Application::GetDefaultDevice(), 1, 1, nBitCount );
}

VirtualDevice::VirtualDevice( const OutputDevice& rCompDev, sal_uInt16 nBitCount )
    : mpVirDev( NULL ),
    meRefDevMode( REFDEV_NONE )
{
    SAL_WARN_IF( nBitCount > 1 && nBitCount != 8, "vcl.gdi",
                 "VirtualDevice::VirtualDevice(): Only 0, 1 or 8 allowed for BitCount, not " << nBitCount );
    SAL_INFO( "vcl.gdi", "VirtualDevice::VirtualDevice( " << nBitCount << " )" );

    ImplInitVirDev( &rCompDev, 1, 1, nBitCount );
}

VirtualDevice::VirtualDevice( const OutputDevice& rCompDev, sal_uInt16 nBitCount, sal_uInt16 nAlphaBitCount )
    : mpVirDev( NULL ),
    meRefDevMode( REFDEV_NONE )
{
    SAL_WARN_IF( nBitCount > 1 && nBitCount != 8, "vcl.gdi",
                 "VirtualDevice::VirtualDevice(): Only 0, 1 or 8 allowed for BitCount, not " << nBitCount );
    SAL_INFO( "vcl.gdi",
            "VirtualDevice::VirtualDevice( " << nBitCount << ", " << nAlphaBitCount << " )" );

    ImplInitVirDev( &rCompDev, 1, 1, nBitCount );

    // Enable alpha channel
    mnAlphaDepth = sal::static_int_cast<sal_Int8>(nAlphaBitCount);
}

VirtualDevice::VirtualDevice( const SystemGraphicsData *pData, sal_uInt16 nBitCount )
:   mpVirDev( NULL ),
    meRefDevMode( REFDEV_NONE )
{
    SAL_INFO( "vcl.gdi", "VirtualDevice::VirtualDevice( " << nBitCount << " )" );

    ImplInitVirDev( Application::GetDefaultDevice(), 1, 1, nBitCount, pData );
}

VirtualDevice::~VirtualDevice()
{
    SAL_INFO( "vcl.gdi", "VirtualDevice::~VirtualDevice()" );

    ImplSVData* pSVData = ImplGetSVData();

    ReleaseGraphics();

    delete mpVirDev;

    // remove this VirtualDevice from the double-linked global list
    if( mpPrev )
        mpPrev->mpNext = mpNext;
    else
        pSVData->maGDIData.mpFirstVirDev = mpNext;

    if( mpNext )
        mpNext->mpPrev = mpPrev;
    else
        pSVData->maGDIData.mpLastVirDev = mpPrev;
}

bool VirtualDevice::InnerImplSetOutputSizePixel( const Size& rNewSize, bool bErase,
                                                 const basebmp::RawMemorySharedArray &pBuffer,
                                                 const bool bTopDown )
{
    SAL_INFO( "vcl.gdi",
              "VirtualDevice::InnerImplSetOutputSizePixel( " << rNewSize.Width() << ", "
              << rNewSize.Height() << ", " << int(bErase) << " )" );

    if ( !mpVirDev )
        return false;
    else if ( rNewSize == GetOutputSizePixel() )
    {
        if ( bErase )
            Erase();
        // Yeah, so trying to re-use a VirtualDevice but this time using a
        // pre-allocated buffer won't work. Big deal.
        return true;
    }

    bool bRet;
    long nNewWidth = rNewSize.Width(), nNewHeight = rNewSize.Height();

    if ( nNewWidth < 1 )
        nNewWidth = 1;

    if ( nNewHeight < 1 )
        nNewHeight = 1;

    if ( bErase )
    {
        if ( pBuffer )
            bRet = mpVirDev->SetSizeUsingBuffer( nNewWidth, nNewHeight, pBuffer, bTopDown );
        else
            bRet = mpVirDev->SetSize( nNewWidth, nNewHeight );

        if ( bRet )
        {
            mnOutWidth  = rNewSize.Width();
            mnOutHeight = rNewSize.Height();
            Erase();
        }
    }
    else
    {
        SalVirtualDevice*   pNewVirDev;
        ImplSVData*         pSVData = ImplGetSVData();

        // we need a graphics
        if ( !mpGraphics )
        {
            if ( !AcquireGraphics() )
                return false;
        }

        pNewVirDev = pSVData->mpDefInst->CreateVirtualDevice( mpGraphics, nNewWidth, nNewHeight, mnBitCount );
        if ( pNewVirDev )
        {
            SalGraphics* pGraphics = pNewVirDev->AcquireGraphics();
            if ( pGraphics )
            {
                SalTwoRect aPosAry;
                long nWidth;
                long nHeight;
                if ( mnOutWidth < nNewWidth )
                    nWidth = mnOutWidth;
                else
                    nWidth = nNewWidth;
                if ( mnOutHeight < nNewHeight )
                    nHeight = mnOutHeight;
                else
                    nHeight = nNewHeight;
                aPosAry.mnSrcX       = 0;
                aPosAry.mnSrcY       = 0;
                aPosAry.mnSrcWidth   = nWidth;
                aPosAry.mnSrcHeight  = nHeight;
                aPosAry.mnDestX      = 0;
                aPosAry.mnDestY      = 0;
                aPosAry.mnDestWidth  = nWidth;
                aPosAry.mnDestHeight = nHeight;

                pGraphics->CopyBits( aPosAry, mpGraphics, this, this );
                pNewVirDev->ReleaseGraphics( pGraphics );
                ReleaseGraphics();
                delete mpVirDev;
                mpVirDev = pNewVirDev;
                mnOutWidth  = rNewSize.Width();
                mnOutHeight = rNewSize.Height();
                bRet = true;
            }
            else
            {
                bRet = false;
                delete pNewVirDev;
            }
        }
        else
            bRet = false;
    }

    return bRet;
}

// #i32109#: Fill opaque areas correctly (without relying on
// fill/linecolor state)
void VirtualDevice::ImplFillOpaqueRectangle( const Rectangle& rRect )
{
    // Set line and fill color to black (->opaque),
    // fill rect with that (linecolor, too, because of
    // those pesky missing pixel problems)
    Push( PUSH_LINECOLOR | PUSH_FILLCOLOR );
    SetLineColor( COL_BLACK );
    SetFillColor( COL_BLACK );
    DrawRect( rRect );
    Pop();
}

bool VirtualDevice::ImplSetOutputSizePixel( const Size& rNewSize, bool bErase,
                                            const basebmp::RawMemorySharedArray &pBuffer,
                                            const bool bTopDown )
{
    if( InnerImplSetOutputSizePixel(rNewSize, bErase, pBuffer, bTopDown) )
    {
        if( mnAlphaDepth != -1 )
        {
            // #110958# Setup alpha bitmap
            if(mpAlphaVDev && mpAlphaVDev->GetOutputSizePixel() != rNewSize)
            {
                delete mpAlphaVDev;
                mpAlphaVDev = 0L;
            }

            if( !mpAlphaVDev )
            {
                mpAlphaVDev = new VirtualDevice( *this, mnAlphaDepth );
                mpAlphaVDev->InnerImplSetOutputSizePixel(rNewSize, bErase,
                                                         basebmp::RawMemorySharedArray(),
                                                         bTopDown );
            }

            // TODO: copy full outdev state to new one, here. Also needed in outdev2.cxx:DrawOutDev
            if( GetLineColor() != Color( COL_TRANSPARENT ) )
                mpAlphaVDev->SetLineColor( COL_BLACK );

            if( GetFillColor() != Color( COL_TRANSPARENT ) )
                mpAlphaVDev->SetFillColor( COL_BLACK );

            mpAlphaVDev->SetMapMode( GetMapMode() );
        }

        return true;
    }

    return false;
}

void VirtualDevice::EnableRTL( bool bEnable )
{
    // virdevs default to not mirroring, they will only be set to mirroring
    // under rare circumstances in the UI, eg the valueset control
    // because each virdev has its own SalGraphics we can safely switch the SalGraphics here
    // ...hopefully
    if( AcquireGraphics() )
        mpGraphics->SetLayout( bEnable ? SAL_LAYOUT_BIDI_RTL : 0 );

    OutputDevice::EnableRTL(bEnable);
}

bool VirtualDevice::SetOutputSizePixel( const Size& rNewSize, bool bErase )
{
    return ImplSetOutputSizePixel( rNewSize, bErase, basebmp::RawMemorySharedArray(), false );
}

bool VirtualDevice::SetOutputSizePixelScaleOffsetAndBuffer(
    const Size& rNewSize, const Fraction& rScale, const Point& rNewOffset,
    const basebmp::RawMemorySharedArray &pBuffer, const bool bTopDown )
{
    if (pBuffer) {
        MapMode mm = GetMapMode();
        mm.SetOrigin( rNewOffset );
        mm.SetScaleX( rScale );
        mm.SetScaleY( rScale );
        SetMapMode( mm );
    }
    return ImplSetOutputSizePixel( rNewSize, true, pBuffer, bTopDown );
}

void VirtualDevice::SetReferenceDevice( RefDevMode i_eRefDevMode )
{
    sal_Int32 nDPIX = 600, nDPIY = 600;
    switch( i_eRefDevMode )
    {
    case REFDEV_NONE:
    default:
        DBG_ASSERT( false, "VDev::SetRefDev illegal argument!" );
        break;
    case REFDEV_MODE06:
        nDPIX = nDPIY = 600;
        break;
    case REFDEV_MODE_MSO1:
        nDPIX = nDPIY = 6*1440;
        break;
    case REFDEV_MODE_PDF1:
        nDPIX = nDPIY = 720;
        break;
    }
    ImplSetReferenceDevice( i_eRefDevMode, nDPIX, nDPIY );
}

void VirtualDevice::SetReferenceDevice( sal_Int32 i_nDPIX, sal_Int32 i_nDPIY )
{
    ImplSetReferenceDevice( REFDEV_CUSTOM, i_nDPIX, i_nDPIY );
}

void VirtualDevice::ImplSetReferenceDevice( RefDevMode i_eRefDevMode, sal_Int32 i_nDPIX, sal_Int32 i_nDPIY )
{
    mnDPIX = i_nDPIX;
    mnDPIY = i_nDPIY;
    mnDPIScaleFactor = 1;

    EnableOutput( false );  // prevent output on reference device
    mbScreenComp = false;

    // invalidate currently selected fonts
    mbInitFont = true;
    mbNewFont = true;

    // avoid adjusting font lists when already in refdev mode
    sal_uInt8 nOldRefDevMode = meRefDevMode;
    sal_uInt8 nOldCompatFlag = (sal_uInt8)meRefDevMode & REFDEV_FORCE_ZERO_EXTLEAD;
    meRefDevMode = (sal_uInt8)(i_eRefDevMode | nOldCompatFlag);
    if( (nOldRefDevMode ^ nOldCompatFlag) != REFDEV_NONE )
        return;

    // the reference device should have only scalable fonts
    // => clean up the original font lists before getting new ones
    if ( mpFontEntry )
    {
        mpFontCache->Release( mpFontEntry );
        mpFontEntry = NULL;
    }
    if ( mpGetDevFontList )
    {
        delete mpGetDevFontList;
        mpGetDevFontList = NULL;
    }
    if ( mpGetDevSizeList )
    {
        delete mpGetDevSizeList;
        mpGetDevSizeList = NULL;
    }

    // preserve global font lists
    ImplSVData* pSVData = ImplGetSVData();
    if( mpFontCollection && (mpFontCollection != pSVData->maGDIData.mpScreenFontList) )
        delete mpFontCollection;
    if( mpFontCache && (mpFontCache != pSVData->maGDIData.mpScreenFontCache) )
        delete mpFontCache;

    // get font list with scalable fonts only
    AcquireGraphics();
    mpFontCollection = pSVData->maGDIData.mpScreenFontList->Clone( true, false );

    // prepare to use new font lists
    mpFontCache = new ImplFontCache();
}

sal_uInt16 VirtualDevice::GetBitCount() const
{
    return mnBitCount;
}

sal_uInt16 VirtualDevice::GetAlphaBitCount() const
{
    if (mpAlphaVDev)
        return mpAlphaVDev->GetBitCount();

    return 0;
}

bool VirtualDevice::UsePolyPolygonForComplexGradient()
{
    return true;
}

void VirtualDevice::Compat_ZeroExtleadBug()
{
    meRefDevMode = (sal_uInt8)meRefDevMode | REFDEV_FORCE_ZERO_EXTLEAD;
}

long VirtualDevice::GetFontExtLeading() const
{
#ifdef UNX
    // backwards compatible line metrics after fixing #i60945#
    if ( ForceZeroExtleadBug() )
        return 0;
#endif

    ImplFontEntry*      pEntry = mpFontEntry;
    ImplFontMetricData* pMetric = &(pEntry->maMetric);

    return pMetric->mnExtLeading;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
