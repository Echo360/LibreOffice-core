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

#include <com/sun/star/task/XStatusIndicator.hpp>
#include <unotools/ucbstreamhelper.hxx>

#include <osl/endian.h>
#include <vcl/virdev.hxx>
#include <vcl/graph.hxx>
#include <tools/stream.hxx>
#include <chart.hxx>
#include <main.hxx>
#include <elements.hxx>
#include <outact.hxx>
#include <boost/scoped_ptr.hpp>

using namespace ::com::sun::star;

CGM::CGM( sal_uInt32 nMode, uno::Reference< frame::XModel > & rModel )
    : mnOutdx(28000)
    , mnOutdy(21000)
    , mnVDCXadd(0)
    , mnVDCYadd(0)
    , mnVDCXmul(0)
    , mnVDCYmul(0)
    , mnVDCdx(0)
    , mnVDCdy(0)
    , mnXFraction(0)
    , mnYFraction(0)
    , mbAngReverse(false)
    , mpGraphic(NULL)
    , mbStatus(true)
    , mbMetaFile(false)
    , mbIsFinished(false)
    , mbPicture(false)
    , mbPictureBody(false)
    , mbFigure(false)
    , mbFirstOutPut(false)
    , mnAct4PostReset(0)
    , mpBitmapInUse(NULL)
    , mpChart(NULL)
    , mpOutAct(new CGMImpressOutAct(*this, rModel))
    , mpSource(NULL)
    , mnParaSize(0)
    , mnActCount(0)
    , mpBuf(NULL)
    , mnMode(nMode | CGM_EXPORT_IMPRESS)
    , mnEscape(0)
    , mnElementClass(0)
    , mnElementID(0)
    , mnElementSize(0)
    , mpVirDev(NULL)
    , mpGDIMetaFile(NULL)
{
    pElement = new CGMElements( *this );
    pCopyOfE = new CGMElements( *this );
}

CGM::~CGM()
{

    if ( mpGraphic )
    {
        mpGDIMetaFile->Stop();
        mpGDIMetaFile->SetPrefMapMode( MapMode() );
        mpGDIMetaFile->SetPrefSize( Size( static_cast< long >( mnOutdx ), static_cast< long >( mnOutdy ) ) );
        delete mpVirDev;
        *mpGraphic = Graphic( *mpGDIMetaFile );
    }
    for( size_t i = 0, n = maDefRepList.size(); i < n; ++i )
        delete maDefRepList[ i ];
    maDefRepList.clear();
    maDefRepSizeList.clear();
    delete mpBitmapInUse;
    delete mpChart;
    delete mpOutAct;
    delete pCopyOfE;
    delete pElement;
    delete [] mpBuf;
};

sal_uInt32 CGM::GetBackGroundColor()
{
    return ( pElement ) ? pElement->aColorTable[ 0 ] : 0;
}

sal_uInt32 CGM::ImplGetUI16( sal_uInt32 /*nAlign*/ )
{
    sal_uInt8* pSource = mpSource + mnParaSize;
    mnParaSize += 2;
    return ( pSource[ 0 ] << 8 ) +  pSource[ 1 ];
};

sal_uInt8 CGM::ImplGetByte( sal_uInt32 nSource, sal_uInt32 nPrecision )
{
    return (sal_uInt8)( nSource >> ( ( nPrecision - 1 ) << 3 ) );
};

sal_Int32 CGM::ImplGetI( sal_uInt32 nPrecision )
{
    sal_uInt8* pSource = mpSource + mnParaSize;
    mnParaSize += nPrecision;
    switch( nPrecision )
    {
        case 1 :
        {
            return  (char)*pSource;
        }

        case 2 :
        {
            return (sal_Int16)( ( pSource[ 0 ] << 8 ) | pSource[ 1 ] );
        }

        case 3 :
        {
            return ( ( pSource[ 0 ] << 24 ) | ( pSource[ 1 ] << 16 ) | pSource[ 2 ] << 8 ) >> 8;
        }
        case 4:
        {
            return (sal_Int32)( ( pSource[ 0 ] << 24 ) | ( pSource[ 1 ] << 16 ) | ( pSource[ 2 ] << 8 ) | ( pSource[ 3 ] ) );
        }
        default:
            mbStatus = false;
            return 0;
    }
}

sal_uInt32 CGM::ImplGetUI( sal_uInt32 nPrecision )
{
    sal_uInt8* pSource = mpSource + mnParaSize;
    mnParaSize += nPrecision;
    switch( nPrecision )
    {
        case 1 :
            return  (sal_Int8)*pSource;
        case 2 :
        {
            return (sal_uInt16)( ( pSource[ 0 ] << 8 ) | pSource[ 1 ] );
        }
        case 3 :
        {
            return ( pSource[ 0 ] << 16 ) | ( pSource[ 1 ] << 8 ) | pSource[ 2 ];
        }
        case 4:
        {
            return (sal_uInt32)( ( pSource[ 0 ] << 24 ) | ( pSource[ 1 ] << 16 ) | ( pSource[ 2 ] << 8 ) | ( pSource[ 3 ] ) );
        }
        default:
            mbStatus = false;
            return 0;
    }
}

void CGM::ImplGetSwitch4( sal_uInt8* pSource, sal_uInt8* pDest )
{
    for ( int i = 0; i < 4; i++ )
    {
        pDest[ i ] = pSource[ i ^ 3 ];          // Little Endian <-> Big Endian switch
    }
}

void CGM::ImplGetSwitch8( sal_uInt8* pSource, sal_uInt8* pDest )
{
    for ( int i = 0; i < 8; i++ )
    {
        pDest[ i ] = pSource[ i ^ 7 ];          // Little Endian <-> Big Endian switch
    }
}

double CGM::ImplGetFloat( RealPrecision eRealPrecision, sal_uInt32 nRealSize )
{
    void*   pPtr;
    sal_uInt8   aBuf[8];
    bool    bCompatible;
    double  nRetValue;
    double  fDoubleBuf;
    float   fFloatBuf;

#ifdef OSL_BIGENDIAN
        bCompatible = sal_True;
#else
        bCompatible = false;
#endif
    if ( bCompatible )
        pPtr = mpSource + mnParaSize;
    else
    {
        if ( nRealSize == 4 )
            ImplGetSwitch4( mpSource + mnParaSize, &aBuf[0] );
        else
            ImplGetSwitch8( mpSource + mnParaSize, &aBuf[0] );
        pPtr = &aBuf;
    }
    if ( eRealPrecision == RP_FLOAT )
    {
        if ( nRealSize == 4 )
        {
            memcpy( (void*)&fFloatBuf, pPtr, 4 );
            nRetValue = (double)fFloatBuf;
        }
        else
        {
            memcpy( (void*)&fDoubleBuf, pPtr, 8 );
            nRetValue = fDoubleBuf;
        }
    }
    else // ->RP_FIXED
    {
        long    nVal;
        int     nSwitch = ( bCompatible ) ? 0 : 1 ;
        if ( nRealSize == 4 )
        {
            sal_uInt16* pShort = (sal_uInt16*)pPtr;
            nVal = pShort[ nSwitch ];
            nVal <<= 16;
            nVal |= pShort[ nSwitch ^ 1 ];
            nRetValue = (double)nVal;
            nRetValue /= 65536;
        }
        else
        {
            sal_Int32* pLong = (sal_Int32*)pPtr;
            nRetValue = (double)abs( pLong[ nSwitch ] );
            nRetValue *= 65536;
            nVal = (sal_uInt32)( pLong[ nSwitch ^ 1 ] );
            nVal >>= 16;
            nRetValue += (double)nVal;
            if ( pLong[ nSwitch ] < 0 )
            {
                nRetValue = -nRetValue;
            }
            nRetValue /= 65536;
        }
    }
    mnParaSize += nRealSize;
    return nRetValue;
}

sal_uInt32 CGM::ImplGetPointSize()
{
    if ( pElement->eVDCType == VDC_INTEGER )
        return pElement->nVDCIntegerPrecision << 1;
    else
        return pElement->nVDCRealSize << 1;
}

inline double CGM::ImplGetIX()
{
    return ( ( ImplGetI( pElement->nVDCIntegerPrecision ) + mnVDCXadd ) * mnVDCXmul );
}

inline double CGM::ImplGetFX()
{
    return ( ( ImplGetFloat( pElement->eVDCRealPrecision, pElement->nVDCRealSize ) + mnVDCXadd ) * mnVDCXmul );
}

inline double CGM::ImplGetIY()
{
    return ( ( ImplGetI( pElement->nVDCIntegerPrecision ) + mnVDCYadd ) * mnVDCYmul );
}

inline double CGM::ImplGetFY()
{
    return ( ( ImplGetFloat( pElement->eVDCRealPrecision, pElement->nVDCRealSize ) + mnVDCYadd ) * mnVDCYmul );
}

void CGM::ImplGetPoint( FloatPoint& rFloatPoint, bool bMap )
{
    if ( pElement->eVDCType == VDC_INTEGER )
    {
        rFloatPoint.X = ImplGetIX();
        rFloatPoint.Y = ImplGetIY();
    }
    else // ->floating points
    {
        rFloatPoint.X = ImplGetFX();
        rFloatPoint.Y = ImplGetFY();
    }
    if ( bMap )
        ImplMapPoint( rFloatPoint );
}

void CGM::ImplGetRectangle( FloatRect& rFloatRect, bool bMap )
{
    if ( pElement->eVDCType == VDC_INTEGER )
    {
        rFloatRect.Left = ImplGetIX();
        rFloatRect.Bottom = ImplGetIY();
        rFloatRect.Right = ImplGetIX();
        rFloatRect.Top = ImplGetIY();
    }
    else // ->floating points
    {
        rFloatRect.Left = ImplGetFX();
        rFloatRect.Bottom = ImplGetFY();
        rFloatRect.Right = ImplGetFX();
        rFloatRect.Top = ImplGetFY();
    }
    if ( bMap )
    {
        ImplMapX( rFloatRect.Left );
        ImplMapX( rFloatRect.Right );
        ImplMapY( rFloatRect.Top );
        ImplMapY( rFloatRect.Bottom );
        rFloatRect.Justify();
    }
}

void CGM::ImplGetRectangleNS( FloatRect& rFloatRect )
{
    if ( pElement->eVDCType == VDC_INTEGER )
    {
        rFloatRect.Left = ImplGetI( pElement->nVDCIntegerPrecision );
        rFloatRect.Bottom = ImplGetI( pElement->nVDCIntegerPrecision );
        rFloatRect.Right = ImplGetI( pElement->nVDCIntegerPrecision );
        rFloatRect.Top = ImplGetI( pElement->nVDCIntegerPrecision );
    }
    else // ->floating points
    {
        rFloatRect.Left = ImplGetFloat( pElement->eVDCRealPrecision, pElement->nVDCRealSize );
        rFloatRect.Bottom = ImplGetFloat( pElement->eVDCRealPrecision, pElement->nVDCRealSize );
        rFloatRect.Right = ImplGetFloat( pElement->eVDCRealPrecision, pElement->nVDCRealSize );
        rFloatRect.Top = ImplGetFloat( pElement->eVDCRealPrecision, pElement->nVDCRealSize );
    }
}

sal_uInt32 CGM::ImplGetBitmapColor( bool bDirect )
{
    // the background color is always a direct color

    sal_uInt32  nTmp;
    if ( ( pElement->eColorSelectionMode == CSM_DIRECT ) || bDirect )
    {
        sal_uInt32      nColor = ImplGetByte( ImplGetUI( pElement->nColorPrecision ), 1 );
        sal_uInt32      nDiff = pElement->nColorValueExtent[ 3 ] - pElement->nColorValueExtent[ 0 ] + 1;

        if ( !nDiff )
            nDiff++;
        nColor = ( ( nColor - pElement->nColorValueExtent[ 0 ] ) << 8 ) / nDiff;
        nTmp = nColor << 16 & 0xff0000;

        nColor = ImplGetByte( ImplGetUI( pElement->nColorPrecision ), 1 );
        nDiff = pElement->nColorValueExtent[ 4 ] - pElement->nColorValueExtent[ 1 ] + 1;
        if ( !nDiff )
            nDiff++;
        nColor = ( ( nColor - pElement->nColorValueExtent[ 1 ] ) << 8 ) / nDiff;
        nTmp |= nColor << 8 & 0xff00;

        nColor = ImplGetByte( ImplGetUI( pElement->nColorPrecision ), 1 );
        nDiff = pElement->nColorValueExtent[ 5 ] - pElement->nColorValueExtent[ 2 ] + 1;
        if ( !nDiff )
            nDiff++;
        nColor = ( ( nColor - pElement->nColorValueExtent[ 2 ] ) << 8 ) / nDiff;
        nTmp |= (sal_uInt8)nColor;
    }
    else
    {
        sal_uInt32 nIndex = ImplGetUI( pElement->nColorIndexPrecision );
        nTmp = pElement->aColorTable[ (sal_uInt8)( nIndex ) ] ;
    }
    return nTmp;
}

// call this function each time after the mapmode settings has been changed
void CGM::ImplSetMapMode()
{
    int nAngReverse = 1;
    mnVDCdx = pElement->aVDCExtent.Right - pElement->aVDCExtent.Left;

    mnVDCXadd = -pElement->aVDCExtent.Left;
    mnVDCXmul = 1;
    if ( mnVDCdx < 0 )
    {
        nAngReverse ^= 1;
        mnVDCdx = -mnVDCdx;
        mnVDCXmul = -1;
    }

    mnVDCdy = pElement->aVDCExtent.Bottom - pElement->aVDCExtent.Top;
    mnVDCYadd = -pElement->aVDCExtent.Top;
    mnVDCYmul = 1;
    if ( mnVDCdy < 0 )
    {
        nAngReverse ^= 1;
        mnVDCdy = -mnVDCdy;
        mnVDCYmul = -1;
    }
    if ( nAngReverse )
        mbAngReverse = true;
    else
        mbAngReverse = false;

    double fQuo1 = mnVDCdx / mnVDCdy;
    double fQuo2 = mnOutdx / mnOutdy;
    if ( fQuo2 < fQuo1 )
    {
        mnXFraction = mnOutdx / mnVDCdx;
        mnYFraction = mnOutdy * ( fQuo2 / fQuo1 ) / mnVDCdy;
    }
    else
    {
        mnXFraction = mnOutdx * ( fQuo1 / fQuo2 ) / mnVDCdx;
        mnYFraction = mnOutdy / mnVDCdy;
    }
}

void CGM::ImplMapDouble( double& nNumb )
{
    if ( pElement->eDeviceViewPortMap == DVPM_FORCED )
    {
        // point is 1mm * ScalingFactor
        switch ( pElement->eDeviceViewPortMode )
        {
            case DVPM_FRACTION :
            {
                nNumb *= ( mnXFraction + mnYFraction ) / 2;
            }
            break;

            case DVPM_METRIC :
            {
                // nNumb *= ( 100 * pElement->nDeviceViewPortScale );
                nNumb *= ( mnXFraction + mnYFraction ) / 2;
                if ( pElement->nDeviceViewPortScale < 0 )
                    nNumb = -nNumb;
            }
            break;

            case DVPM_DEVICE :
            {

            }
            break;

            default:

                break;
        }
    }
    else
    {


    }
}

void CGM::ImplMapX( double& nNumb )
{
    if ( pElement->eDeviceViewPortMap == DVPM_FORCED )
    {
        // point is 1mm * ScalingFactor
        switch ( pElement->eDeviceViewPortMode )
        {
            case DVPM_FRACTION :
            {
                nNumb *= mnXFraction;
            }
            break;

            case DVPM_METRIC :
            {
                // nNumb *= ( 100 * pElement->nDeviceViewPortScale );
                nNumb *= mnXFraction;
                if ( pElement->nDeviceViewPortScale < 0 )
                    nNumb = -nNumb;
            }
            break;

            case DVPM_DEVICE :
            {

            }
            break;

            default:

                break;
        }
    }
    else
    {


    }
}

void CGM::ImplMapY( double& nNumb )
{
    if ( pElement->eDeviceViewPortMap == DVPM_FORCED )
    {
        // point is 1mm * ScalingFactor
        switch ( pElement->eDeviceViewPortMode )
        {
            case DVPM_FRACTION :
            {
                nNumb *= mnYFraction;
            }
            break;

            case DVPM_METRIC :
            {
                // nNumb *= ( 100 * pElement->nDeviceViewPortScale );
                nNumb *= mnYFraction;
                if ( pElement->nDeviceViewPortScale < 0 )
                    nNumb = -nNumb;
            }
            break;

            case DVPM_DEVICE :
            {

            }
            break;

            default:

                break;
        }
    }
    else
    {


    }
}

// convert a point to the current VC mapmode (1/100TH mm)
void CGM::ImplMapPoint( FloatPoint& rFloatPoint )
{
    if ( pElement->eDeviceViewPortMap == DVPM_FORCED )
    {
        // point is 1mm * ScalingFactor
        switch ( pElement->eDeviceViewPortMode )
        {
            case DVPM_FRACTION :
            {
                rFloatPoint.X *= mnXFraction;
                rFloatPoint.Y *= mnYFraction;
            }
            break;

            case DVPM_METRIC :
            {
                rFloatPoint.X *= mnXFraction;
                rFloatPoint.Y *= mnYFraction;
                if ( pElement->nDeviceViewPortScale < 0 )
                {
                    rFloatPoint.X = -rFloatPoint.X;
                    rFloatPoint.Y = -rFloatPoint.Y;
                }
            }
            break;

            case DVPM_DEVICE :
            {

            }
            break;

            default:

                break;
        }
    }
    else
    {


    }
}

void CGM::ImplDoClass()
{
    switch ( mnElementClass )
    {
        case 0 : ImplDoClass0(); break;
        case 1 : ImplDoClass1(); break;
        case 2 : ImplDoClass2(); break;
        case 3 : ImplDoClass3(); break;
        case 4 :
        {
            ImplDoClass4();
            mnAct4PostReset = 0;
        }
        break;
        case 5 : ImplDoClass5(); break;
        case 6 : ImplDoClass6(); break;
        case 7 : ImplDoClass7(); break;
        case 8 : ImplDoClass8(); break;
        case 9 : ImplDoClass9(); break;
        case 15 :ImplDoClass15(); break;
        default: break;
    }
    mnActCount++;
};

void CGM::ImplDefaultReplacement()
{
    if ( !maDefRepList.empty() )
    {
        sal_uInt32  nOldEscape = mnEscape;
        sal_uInt32  nOldElementClass = mnElementClass;
        sal_uInt32  nOldElementID = mnElementID;
        sal_uInt32  nOldElementSize = mnElementSize;
        sal_uInt8*  pOldBuf = mpSource;

        for ( size_t i = 0, n = maDefRepList.size(); i < n; ++i )
        {
            sal_uInt8*  pBuf = maDefRepList[ i ];
            sal_uInt32  nElementSize = maDefRepSizeList[ i ];
            sal_uInt32  nCount = 0;
            while ( mbStatus && ( nCount < nElementSize ) )
            {
                mpSource = pBuf + nCount;
                mnParaSize = 0;
                mnEscape = ImplGetUI16();
                mnElementClass = mnEscape >> 12;
                mnElementID = ( mnEscape & 0x0fe0 ) >> 5;
                mnElementSize = mnEscape & 0x1f;
                if ( mnElementSize == 31 )
                {
                    mnElementSize = ImplGetUI16();
                }
                nCount += mnParaSize;
                mnParaSize = 0;
                mpSource = pBuf + nCount;
                if ( mnElementSize & 1 )
                    nCount++;
                nCount += mnElementSize;
                if ( ( mnElementClass != 1 ) || ( mnElementID != 0xc ) )    // recursion is not possible here!!
                    ImplDoClass();
            }
        }
        mnEscape = nOldEscape;
        mnElementClass = nOldElementClass;
        mnElementID = nOldElementID;
        mnParaSize = mnElementSize = nOldElementSize;
        mpSource = pOldBuf;
    }
}

bool CGM::Write( SvStream& rIStm )
{
    if ( !mpBuf )
        mpBuf = new sal_uInt8[ 0xffff ];

    mnParaSize = 0;
    mpSource = mpBuf;
    rIStm.Read( mpSource, 2 );
    mnEscape = ImplGetUI16();
    mnElementClass = mnEscape >> 12;
    mnElementID = ( mnEscape & 0x0fe0 ) >> 5;
    mnElementSize = mnEscape & 0x1f;

    if ( mnElementSize == 31 )
    {
        rIStm.Read( mpSource + mnParaSize, 2 );
        mnElementSize = ImplGetUI16();
    }
    mnParaSize = 0;
    if ( mnElementSize )
        rIStm.Read( mpSource + mnParaSize, mnElementSize );

    if ( mnElementSize & 1 )
        rIStm.SeekRel( 1 );
    ImplDoClass();

    return mbStatus;
};

// GraphicImport - the exported function
extern "C" SAL_DLLPUBLIC_EXPORT sal_uInt32 SAL_CALL
ImportCGM( OUString& rFileName, uno::Reference< frame::XModel > & rXModel, sal_uInt32 nMode, void* pProgressBar )
{

    sal_uInt32  nStatus = 0;            // retvalue == 0 -> ERROR
                                        //          == 0xffrrggbb -> background color in the lower 24 bits
    bool    bProgressBar = false;

    if( rXModel.is() )
    {
        try
        {
            boost::scoped_ptr<CGM> pCGM(new CGM( nMode, rXModel ));
            if ( pCGM && pCGM->IsValid() )
            {
                if ( nMode & CGM_IMPORT_CGM )
                {
                    boost::scoped_ptr<SvStream> pIn(::utl::UcbStreamHelper::CreateStream( rFileName, STREAM_READ ));
                    if ( pIn )
                    {
                        pIn->SetNumberFormatInt( NUMBERFORMAT_INT_BIGENDIAN );
                        sal_uInt64 const nInSize = pIn->remainingSize();
                        pIn->Seek( 0 );

                        uno::Reference< task::XStatusIndicator >  aXStatInd;
                        sal_uInt32  nNext = 0;
                        sal_uInt32  nAdd = nInSize / 20;
                        if ( pProgressBar )
                            aXStatInd = *(uno::Reference< task::XStatusIndicator > *)pProgressBar;
                        bProgressBar = aXStatInd.is();
                        if ( bProgressBar )
                            aXStatInd->start( "CGM Import" , nInSize );

                        while ( pCGM->IsValid() && ( pIn->Tell() < nInSize ) && !pCGM->IsFinished() )
                        {
                            if ( bProgressBar )
                            {
                                sal_uInt32 nCurrentPos = pIn->Tell();
                                if ( nCurrentPos >= nNext )
                                {
                                    aXStatInd->setValue( nCurrentPos );
                                    nNext = nCurrentPos + nAdd;
                                }
                            }

                            if ( pCGM->Write( *pIn ) == false )
                                break;
                        }
                        if ( pCGM->IsValid() )
                        {
                            nStatus = pCGM->GetBackGroundColor() | 0xff000000;
                        }
                        if ( bProgressBar )
                            aXStatInd->end();
                    }
                }
            }
        }
        catch( const ::com::sun::star::uno::Exception& )
        {
            nStatus = 0;
        }
    }
    return nStatus;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
