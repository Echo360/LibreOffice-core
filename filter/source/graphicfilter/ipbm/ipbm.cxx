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


#include <vcl/graph.hxx>
#include <vcl/bmpacc.hxx>

class FilterConfigItem;

//============================ PBMReader ==================================

class PBMReader {

private:

    SvStream&           mrPBM;              // the PBM file to read

    bool            mbStatus;
    bool            mbRemark;           // sal_False if the stream is in a comment
    bool            mbRaw;              // RAW/ASCII MODE
    sal_uLong           mnMode;             // 0->PBM, 1->PGM, 2->PPM
    Bitmap              maBmp;
    BitmapWriteAccess*  mpAcc;
    sal_uLong           mnWidth, mnHeight;  // dimensions in pixel
    sal_uLong           mnCol;
    sal_uLong           mnMaxVal;           // max value in the <missing comment>
    bool            ImplCallback( sal_uInt16 nPercent );
    bool            ImplReadBody();
    bool            ImplReadHeader();

public:
                        PBMReader(SvStream & rPBM);
                        ~PBMReader();
    bool                ReadPBM(Graphic & rGraphic );
};

//=================== Methods of PBMReader ==============================

PBMReader::PBMReader(SvStream & rPBM)
    : mrPBM(rPBM)
    , mbStatus(true)
    , mbRemark(false)
    , mbRaw(true)
    , mnMode(0)
    , mpAcc(NULL)
    , mnWidth(0)
    , mnHeight(0)
    , mnCol(0)
    , mnMaxVal(0)
{
}

PBMReader::~PBMReader()
{
}

bool PBMReader::ImplCallback( sal_uInt16 /*nPercent*/ )
{
/*
    if ( pCallback != NULL )
    {
        if ( ( (*pCallback)( pCallerData, nPercent ) ) == sal_True )
        {
            mrPBM.SetError( SVSTREAM_FILEFORMAT_ERROR );
            return sal_True;
        }
    }
*/
    return false;
}

bool PBMReader::ReadPBM(Graphic & rGraphic )
{
    sal_uInt16 i;

    if ( mrPBM.GetError() )
        return false;

    mrPBM.SetNumberFormatInt( NUMBERFORMAT_INT_LITTLEENDIAN );

    // read header:

    if ( ( mbStatus = ImplReadHeader() ) == false )
        return false;

    if ( ( mnMaxVal == 0 ) || ( mnWidth == 0 ) || ( mnHeight == 0 ) )
        return false;

    // 0->PBM, 1->PGM, 2->PPM
    switch ( mnMode )
    {
        case 0 :
            maBmp = Bitmap( Size( mnWidth, mnHeight ), 1 );
            if ( ( mpAcc = maBmp.AcquireWriteAccess() ) == 0 )
                return false;
            mpAcc->SetPaletteEntryCount( 2 );
            mpAcc->SetPaletteColor( 0, BitmapColor( 0xff, 0xff, 0xff ) );
            mpAcc->SetPaletteColor( 1, BitmapColor( 0x00, 0x00, 0x00 ) );
            break;

        case 1 :
            if ( mnMaxVal <= 1 )
                maBmp = Bitmap( Size( mnWidth, mnHeight ), 1);
            else if ( mnMaxVal <= 15 )
                maBmp = Bitmap( Size( mnWidth, mnHeight ), 4);
            else
                maBmp = Bitmap( Size( mnWidth, mnHeight ), 8);

            if ( ( mpAcc = maBmp.AcquireWriteAccess() ) == 0 )
                return false;
            mnCol = (sal_uInt16)mnMaxVal + 1;
            if ( mnCol > 256 )
                mnCol = 256;

            mpAcc->SetPaletteEntryCount( 256 );
            for ( i = 0; i < mnCol; i++ )
            {
                sal_uLong nCount = 255 * i / mnCol;
                mpAcc->SetPaletteColor( i, BitmapColor( (sal_uInt8)nCount, (sal_uInt8)nCount, (sal_uInt8)nCount ) );
            }
            break;
        case 2 :
            maBmp = Bitmap( Size( mnWidth, mnHeight ), 24 );
            if ( ( mpAcc = maBmp.AcquireWriteAccess() ) == 0 )
                return false;
            break;
    }

    // read bitmap data
    mbStatus = ImplReadBody();

    if ( mpAcc )
    {
        maBmp.ReleaseAccess( mpAcc ), mpAcc = NULL;
    }
    if ( mbStatus )
        rGraphic = maBmp;

    return mbStatus;
}

bool PBMReader::ImplReadHeader()
{
    sal_uInt8   nID[ 2 ];
    sal_uInt8   nDat;
    sal_uInt8   nMax, nCount = 0;
    bool    bFinished = false;

    mrPBM.ReadUChar( nID[ 0 ] ).ReadUChar( nID[ 1 ] );
    if ( nID[ 0 ] != 'P' )
        return false;
    mnMaxVal = mnWidth = mnHeight = 0;
    switch ( nID[ 1 ] )
    {
        case '1' :
            mbRaw = false;
            //fall-through
        case '4' :
            mnMode = 0;
            nMax = 2;               // number of parameters in Header
            mnMaxVal = 1;
            break;
        case '2' :
            mbRaw = false;
            //fall-through
        case '5' :
            mnMode = 1;
            nMax = 3;
            break;
        case '3' :
            mbRaw = false;
            //fall-through
        case '6' :
            mnMode = 2;
            nMax = 3;
            break;
        default:
            return false;
    }
    while ( bFinished == false )
    {
        if ( mrPBM.GetError() )
            return false;

        mrPBM.ReadUChar( nDat );

        if ( nDat == '#' )
        {
            mbRemark = true;
            continue;
        }
        else if ( ( nDat == 0x0d ) || ( nDat == 0x0a ) )
        {
            mbRemark = false;
            nDat = 0x20;
        }
        if ( mbRemark )
            continue;

        if ( ( nDat == 0x20 ) || ( nDat == 0x09 ) )
        {
            if ( ( nCount == 0 ) && mnWidth )
                nCount++;
            else if ( ( nCount == 1 ) && mnHeight )
            {
                if ( ++nCount == nMax )
                    bFinished = true;
            }
            else if ( ( nCount == 2 ) && mnMaxVal )
            {
                bFinished = true;
            }
            continue;
        }
        if ( ( nDat >= '0' ) && ( nDat <= '9' ) )
        {
            nDat -= '0';
            if ( nCount == 0 )
            {
                mnWidth *= 10;
                mnWidth += nDat;
            }
            else if ( nCount == 1 )
            {
                mnHeight *= 10;
                mnHeight += nDat;
            }
            else if ( nCount == 2 )
            {
                mnMaxVal *= 10;
                mnMaxVal += nDat;
            }
        }
        else
            return false;
    }
    return mbStatus;
}

bool PBMReader::ImplReadBody()
{
    bool    bPara, bFinished = false;
    sal_uInt8   nDat = 0, nCount;
    sal_uLong   nGrey, nRGB[3];
    sal_uLong   nWidth = 0;
    sal_uLong   nHeight = 0;

    if ( mbRaw )
    {
        signed char nShift = 0;
        switch ( mnMode )
        {

            // PBM
            case 0 :
                while ( nHeight != mnHeight )
                {
                    if ( mrPBM.IsEof() || mrPBM.GetError() )
                        return false;

                    if ( --nShift < 0 )
                    {
                        mrPBM.ReadUChar( nDat );
                        nShift = 7;
                    }
                    mpAcc->SetPixelIndex( nHeight, nWidth, nDat >> nShift );
                    if ( ++nWidth == mnWidth )
                    {
                        nShift = 0;
                        nWidth = 0;
                        nHeight++;
                        ImplCallback( (sal_uInt16)( ( 100 * nHeight ) / mnHeight ) );   // processing output in percent
                    }
                }
                break;

            // PGM
            case 1 :
                while ( nHeight != mnHeight )
                {
                    if ( mrPBM.IsEof() || mrPBM.GetError() )
                        return false;

                    mrPBM.ReadUChar( nDat );
                    mpAcc->SetPixelIndex( nHeight, nWidth++, nDat);

                    if ( nWidth == mnWidth )
                    {
                        nWidth = 0;
                        nHeight++;
                        ImplCallback( (sal_uInt16)( ( 100 * nHeight ) / mnHeight ) );   // processing output in percent
                    }
                }
                break;

            // PPM
            case 2 :
                while ( nHeight != mnHeight )
                {
                    if ( mrPBM.IsEof() || mrPBM.GetError() )
                        return false;

                    sal_uInt8   nR, nG, nB;
                    sal_uLong   nRed, nGreen, nBlue;
                    mrPBM.ReadUChar( nR ).ReadUChar( nG ).ReadUChar( nB );
                    nRed = 255 * nR / mnMaxVal;
                    nGreen = 255 * nG / mnMaxVal;
                    nBlue = 255 * nB / mnMaxVal;
                    mpAcc->SetPixel( nHeight, nWidth++, BitmapColor( (sal_uInt8)nRed, (sal_uInt8)nGreen, (sal_uInt8)nBlue ) );
                    if ( nWidth == mnWidth )
                    {
                        nWidth = 0;
                        nHeight++;
                        ImplCallback( (sal_uInt16) ( ( 100 * nHeight ) / mnHeight ) );  // processing output in percent
                    }
                }
                break;
        }
    }
    else switch  ( mnMode )
    {
        // PBM
        case 0 :
            while ( bFinished == false )
            {
                if ( mrPBM.IsEof() || mrPBM.GetError() )
                    return false;

                mrPBM.ReadUChar( nDat );

                if ( nDat == '#' )
                {
                    mbRemark = true;
                    continue;
                }
                else if ( ( nDat == 0x0d ) || ( nDat == 0x0a ) )
                {
                    mbRemark = false;
                    continue;
                }
                if ( mbRemark || nDat == 0x20 || nDat == 0x09 )
                    continue;

                if ( nDat == '0' || nDat == '1' )
                {
                    mpAcc->SetPixelIndex( nHeight, nWidth, static_cast<sal_uInt8>(nDat - '0') );
                    nWidth++;
                    if ( nWidth == mnWidth )
                    {
                        nWidth = 0;
                        if ( ++nHeight == mnHeight )
                            bFinished = true;
                        ImplCallback( (sal_uInt16) ( ( 100 * nHeight ) / mnHeight ) );  // processing output in percent
                    }
                }
                else
                    return false;
            }
            break;

        // PGM
        case 1 :

            bPara = false;
            nCount = 0;
            nGrey = 0;

            while ( bFinished == false )
            {
                if ( nCount )
                {
                    nCount--;
                    if ( nGrey <= mnMaxVal )
                        nGrey = 255 * nGrey / mnMaxVal;
                    mpAcc->SetPixelIndex( nHeight, nWidth++, static_cast<sal_uInt8>(nGrey) );
                    nGrey = 0;
                    if ( nWidth == mnWidth )
                    {
                        nWidth = 0;
                        if ( ++nHeight == mnHeight )
                            bFinished = true;
                        ImplCallback( (sal_uInt16) ( ( 100 * nHeight ) / mnHeight ) );  // processing output in percent
                    }
                    continue;
                }

                if ( mrPBM.IsEof() || mrPBM.GetError() )
                    return false;

                mrPBM.ReadUChar( nDat );

                if ( nDat == '#' )
                {
                    mbRemark = true;
                    if ( bPara )
                    {
                        bPara = false;
                        nCount++;
                    }
                    continue;
                }
                else if ( ( nDat == 0x0d ) || ( nDat == 0x0a ) )
                {
                    mbRemark = false;
                    if ( bPara )
                    {
                        bPara = false;
                        nCount++;
                    }
                    continue;
                }

                if ( nDat == 0x20 || nDat == 0x09 )
                {
                    if ( bPara )
                    {
                        bPara = false;
                        nCount++;
                    }
                    continue;
                }
                if ( nDat >= '0' && nDat <= '9' )
                {
                    bPara = true;
                    nGrey *= 10;
                    nGrey += nDat-'0';
                    continue;
                }
                else
                    return false;
            }
            break;



        // PPM
        case 2 :

            bPara = false;
            nCount = 0;
            nRGB[ 0 ] = nRGB[ 1 ] = nRGB[ 2 ] = 0;

            while ( bFinished == false )
            {
                if ( nCount == 3 )
                {
                    nCount = 0;
                    mpAcc->SetPixel( nHeight, nWidth++, BitmapColor( static_cast< sal_uInt8 >( ( nRGB[ 0 ] * 255 ) / mnMaxVal ),
                                                                     static_cast< sal_uInt8 >( ( nRGB[ 1 ] * 255 ) / mnMaxVal ),
                                                                     static_cast< sal_uInt8 >( ( nRGB[ 2 ] * 255 ) / mnMaxVal ) ) );
                    nRGB[ 0 ] = nRGB[ 1 ] = nRGB[ 2 ] = 0;
                    if ( nWidth == mnWidth )
                    {
                        nWidth = 0;
                        if ( ++nHeight == mnHeight )
                            bFinished = true;
                        ImplCallback( (sal_uInt16) ( ( 100 * nHeight ) / mnHeight ) );  // processing output in percent
                    }
                    continue;
                }

                if ( mrPBM.IsEof() || mrPBM.GetError() )
                    return false;

                mrPBM.ReadUChar( nDat );

                if ( nDat == '#' )
                {
                    mbRemark = true;
                    if ( bPara )
                    {
                        bPara = false;
                        nCount++;
                    }
                    continue;
                }
                else if ( ( nDat == 0x0d ) || ( nDat == 0x0a ) )
                {
                    mbRemark = false;
                    if ( bPara )
                    {
                        bPara = false;
                        nCount++;
                    }
                    continue;
                }

                if ( nDat == 0x20 || nDat == 0x09 )
                {
                    if ( bPara )
                    {
                        bPara = false;
                        nCount++;
                    }
                    continue;
                }
                if ( nDat >= '0' && nDat <= '9' )
                {
                    bPara = true;
                    nRGB[ nCount ] *= 10;
                    nRGB[ nCount ] += nDat-'0';
                    continue;
                }
                else
                    return false;
            }
            break;
    }
    return mbStatus;
}

//================== GraphicImport - the exported function ================

// this needs to be kept in sync with
// ImpFilterLibCacheEntry::GetImportFunction() from
// vcl/source/filter/graphicfilter.cxx
#if defined(DISABLE_DYNLOADING)
#define GraphicImport ipbGraphicImport
#endif

extern "C" SAL_DLLPUBLIC_EXPORT bool SAL_CALL
GraphicImport( SvStream & rStream, Graphic & rGraphic, FilterConfigItem* )
{
    PBMReader aPBMReader(rStream);

    return aPBMReader.ReadPBM(rGraphic );
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
