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

#include <config_folders.h>
#include "rtl/bootstrap.hxx"
#include "oox/core/xmlfilterbase.hxx"
#include "oox/export/drawingml.hxx"
#include "oox/export/utils.hxx"
#include <oox/drawingml/color.hxx>
#include <oox/drawingml/fillproperties.hxx>
#include <oox/token/tokens.hxx>
#include <oox/drawingml/drawingmltypes.hxx>

#include <cstdio>
#include <com/sun/star/awt/CharSet.hpp>
#include <com/sun/star/awt/FontDescriptor.hpp>
#include <com/sun/star/awt/FontSlant.hpp>
#include <com/sun/star/awt/FontStrikeout.hpp>
#include <com/sun/star/awt/FontWeight.hpp>
#include <com/sun/star/awt/FontUnderline.hpp>
#include <com/sun/star/awt/Gradient.hpp>
#include <com/sun/star/beans/XPropertySet.hpp>
#include <com/sun/star/beans/XPropertyState.hpp>
#include <com/sun/star/beans/Property.hpp>
#include <com/sun/star/beans/XPropertySetInfo.hpp>
#include <com/sun/star/container/XEnumerationAccess.hpp>
#include <com/sun/star/container/XIndexAccess.hpp>
#include <com/sun/star/drawing/BitmapMode.hpp>
#include <com/sun/star/drawing/EnhancedCustomShapeAdjustmentValue.hpp>
#include <com/sun/star/drawing/LineDash.hpp>
#include <com/sun/star/drawing/LineJoint.hpp>
#include <com/sun/star/drawing/LineStyle.hpp>
#include <com/sun/star/drawing/TextHorizontalAdjust.hpp>
#include <com/sun/star/drawing/TextVerticalAdjust.hpp>
#include <com/sun/star/drawing/XShape.hpp>
#include <com/sun/star/drawing/FillStyle.hpp>
#include <com/sun/star/geometry/IntegerRectangle2D.hpp>
#include <com/sun/star/i18n/ScriptType.hpp>
#include <com/sun/star/io/XOutputStream.hpp>
#include <com/sun/star/style/LineSpacing.hpp>
#include <com/sun/star/style/LineSpacingMode.hpp>
#include <com/sun/star/style/ParagraphAdjust.hpp>
#include <com/sun/star/text/WritingMode.hpp>
#include <com/sun/star/text/GraphicCrop.hpp>
#include <com/sun/star/text/XText.hpp>
#include <com/sun/star/text/XTextContent.hpp>
#include <com/sun/star/text/XTextField.hpp>
#include <com/sun/star/text/XTextRange.hpp>
#include <tools/stream.hxx>
#include <unotools/fontdefs.hxx>
#include <vcl/cvtgrf.hxx>
#include <vcl/graph.hxx>
#include <vcl/settings.hxx>
#include <svtools/grfmgr.hxx>
#include <rtl/strbuf.hxx>
#include <sfx2/app.hxx>
#include <svl/languageoptions.hxx>
#include <filter/msfilter/escherex.hxx>
#include <filter/msfilter/util.hxx>
#include <editeng/outlobj.hxx>
#include <editeng/svxenum.hxx>
#include <svx/unoapi.hxx>
#include <svx/svdoashp.hxx>
#include <svx/unoshape.hxx>

using namespace ::com::sun::star;
using namespace ::com::sun::star::beans;
using namespace ::com::sun::star::drawing;
using namespace ::com::sun::star::i18n;
using namespace ::com::sun::star::style;
using namespace ::com::sun::star::text;
using namespace ::com::sun::star::uno;
using ::com::sun::star::beans::PropertyState;
using ::com::sun::star::beans::PropertyValue;
using ::com::sun::star::beans::XPropertySet;
using ::com::sun::star::beans::XPropertyState;
using ::com::sun::star::container::XEnumeration;
using ::com::sun::star::container::XEnumerationAccess;
using ::com::sun::star::container::XIndexAccess;
using ::com::sun::star::geometry::IntegerRectangle2D;
using ::com::sun::star::io::XOutputStream;
using ::com::sun::star::style::LineSpacing;
using ::com::sun::star::text::XText;
using ::com::sun::star::text::XTextContent;
using ::com::sun::star::text::XTextField;
using ::com::sun::star::text::XTextRange;
using ::sax_fastparser::FSHelperPtr;

namespace oox {
namespace drawingml {

#define GETA(propName) \
    GetProperty( rXPropSet, OUString( #propName ) )

#define GETAD(propName) \
    ( GetPropertyAndState( rXPropSet, rXPropState, OUString( #propName ), eState ) && eState == beans::PropertyState_DIRECT_VALUE )

#define GET(variable, propName) \
    if ( GETA(propName) ) \
        mAny >>= variable;

// not thread safe
int DrawingML::mnImageCounter = 1;
int DrawingML::mnWdpImageCounter = 1;
std::map<OUString, OUString> DrawingML::maWdpCache;

void DrawingML::ResetCounters()
{
    mnImageCounter = 1;
    mnWdpImageCounter = 1;
    maWdpCache.clear();
}

bool DrawingML::GetProperty( Reference< XPropertySet > rXPropSet, const OUString& aName )
{
    bool bRetValue = false;

    try {
        mAny = rXPropSet->getPropertyValue( aName );
        if ( mAny.hasValue() )
            bRetValue = true;
    } catch( const Exception& ) { /* printf ("exception when trying to get value of property: %s\n", USS(aName)); */ }

    return bRetValue;
}

bool DrawingML::GetPropertyAndState( Reference< XPropertySet > rXPropSet, Reference< XPropertyState > rXPropState, const OUString& aName, PropertyState& eState )
{
    bool bRetValue = false;

    try {
        mAny = rXPropSet->getPropertyValue( aName );
        if ( mAny.hasValue() ) {
            bRetValue = true;
            eState = rXPropState->getPropertyState( aName );
        }
    } catch( const Exception& ) { /* printf ("exception when trying to get value of property: %s\n", USS(aName)); */ }

    return bRetValue;
}

void DrawingML::WriteColor( sal_uInt32 nColor, sal_Int32 nAlpha )
{
    OString sColor = OString::number(  nColor, 16 );
    if( sColor.getLength() < 6 ) {
        OStringBuffer sBuf( "0" );
        int remains = 5 - sColor.getLength();

        while( remains > 0 ) {
            sBuf.append( "0" );
            remains--;
        }

        sBuf.append( sColor );

        sColor = sBuf.getStr();
    }
    if( nAlpha < MAX_PERCENT )
    {
        mpFS->startElementNS( XML_a, XML_srgbClr, XML_val, sColor.getStr(), FSEND );
        mpFS->singleElementNS( XML_a, XML_alpha, XML_val, OString::number(nAlpha), FSEND );
        mpFS->endElementNS( XML_a, XML_srgbClr );

    }
    else
    {
        mpFS->singleElementNS( XML_a, XML_srgbClr, XML_val, sColor.getStr(), FSEND );
    }
}

void DrawingML::WriteColor( const OUString& sColorSchemeName, const Sequence< PropertyValue >& aTransformations )
{
    // prevent writing a tag with empty val attribute
    if( sColorSchemeName.isEmpty() )
        return;

    if( aTransformations.hasElements() )
    {
        mpFS->startElementNS( XML_a, XML_schemeClr,
                              XML_val, USS( sColorSchemeName ),
                              FSEND );
        WriteColorTransformations( aTransformations );
        mpFS->endElementNS( XML_a, XML_schemeClr );
    }
    else
        mpFS->singleElementNS( XML_a, XML_schemeClr,
                              XML_val, USS( sColorSchemeName ),
                              FSEND );
}

void DrawingML::WriteColorTransformations( const Sequence< PropertyValue >& aTransformations )
{
    for( sal_Int32 i = 0; i < aTransformations.getLength(); i++ )
    {
        sal_Int32 nToken = Color::getColorTransformationToken( aTransformations[i].Name );
        if( nToken != XML_TOKEN_INVALID && aTransformations[i].Value.hasValue() )
        {
            sal_Int32 nValue = aTransformations[i].Value.get<sal_Int32>();
            mpFS->singleElementNS( XML_a, nToken, XML_val, I32S( nValue ), FSEND );
        }
    }
}

void DrawingML::WriteSolidFill( sal_uInt32 nColor, sal_Int32 nAlpha )
{
    mpFS->startElementNS( XML_a, XML_solidFill, FSEND );
    WriteColor( nColor, nAlpha );
    mpFS->endElementNS( XML_a, XML_solidFill );
}

void DrawingML::WriteSolidFill( const OUString& sSchemeName, const Sequence< PropertyValue >& aTransformations )
{
    mpFS->startElementNS( XML_a, XML_solidFill, FSEND );
    WriteColor( sSchemeName, aTransformations );
    mpFS->endElementNS( XML_a, XML_solidFill );
}

void DrawingML::WriteSolidFill( Reference< XPropertySet > rXPropSet )
{
    // get fill color
    if ( !GetProperty( rXPropSet, "FillColor" ) )
        return;
    sal_uInt32 nFillColor = mAny.get<sal_uInt32>();

    // get InteropGrabBag and search the relevant attributes
    OUString sColorFillScheme;
    sal_uInt32 nOriginalColor = 0;
    Sequence< PropertyValue > aStyleProperties, aTransformations;
    if ( GetProperty( rXPropSet, "InteropGrabBag" ) )
    {
        Sequence< PropertyValue > aGrabBag;
        mAny >>= aGrabBag;
        for( sal_Int32 i=0; i < aGrabBag.getLength(); ++i )
            if( aGrabBag[i].Name == "SpPrSolidFillSchemeClr" )
                aGrabBag[i].Value >>= sColorFillScheme;
            else if( aGrabBag[i].Name == "OriginalSolidFillClr" )
                aGrabBag[i].Value >>= nOriginalColor;
            else if( aGrabBag[i].Name == "StyleFillRef" )
                aGrabBag[i].Value >>= aStyleProperties;
            else if( aGrabBag[i].Name == "SpPrSolidFillSchemeClrTransformations" )
                aGrabBag[i].Value >>= aTransformations;
    }

    sal_Int32 nAlpha = MAX_PERCENT;
    if( GetProperty( rXPropSet, "FillTransparence" ) )
    {
        sal_Int32 nTransparency = 0;
        mAny >>= nTransparency;
        // Calculate alpha value (see oox/source/drawingml/color.cxx : getTransparency())
        nAlpha = (MAX_PERCENT - ( PER_PERCENT * nTransparency ) );
    }

    // write XML
    if ( nFillColor != nOriginalColor )
        // the user has set a different color for the shape
        WriteSolidFill( nFillColor & 0xffffff, nAlpha );
    else if ( !sColorFillScheme.isEmpty() )
        // the shape had a scheme color and the user didn't change it
        WriteSolidFill( sColorFillScheme, aTransformations );
    else if ( aStyleProperties.hasElements() )
    {
        sal_uInt32 nThemeColor = 0;
        for( sal_Int32 i=0; i < aStyleProperties.getLength(); ++i )
            if( aStyleProperties[i].Name == "Color" )
            {
                aStyleProperties[i].Value >>= nThemeColor;
                break;
            }
        if ( nFillColor != nThemeColor )
            // the shape contains a theme but it wasn't being used
            WriteSolidFill( nFillColor & 0xffffff, nAlpha );
        // in case the shape used the style color and the user didn't change it,
        // we must not write a <a: solidFill> tag.
    }
    else
        // the shape had a custom color and the user didn't change it
        WriteSolidFill( nFillColor & 0xffffff, nAlpha );
}

void DrawingML::WriteGradientStop( sal_uInt16 nStop, sal_uInt32 nColor )
{
    mpFS->startElementNS( XML_a, XML_gs,
                          XML_pos, I32S( nStop * 1000 ),
                          FSEND );
    WriteColor( nColor );
    mpFS->endElementNS( XML_a, XML_gs );
}

sal_uInt32 DrawingML::ColorWithIntensity( sal_uInt32 nColor, sal_uInt32 nIntensity )
{
    return ( ( ( nColor & 0xff ) * nIntensity ) / 100 )
        | ( ( ( ( ( nColor & 0xff00 ) >> 8 ) * nIntensity ) / 100 ) << 8 )
        | ( ( ( ( ( nColor & 0xff0000 ) >> 8 ) * nIntensity ) / 100 ) << 8 );
}

bool DrawingML::EqualGradients( awt::Gradient aGradient1, awt::Gradient aGradient2 )
{
    return aGradient1.Style == aGradient2.Style &&
            aGradient1.StartColor == aGradient2.StartColor &&
            aGradient1.EndColor == aGradient2.EndColor &&
            aGradient1.Angle == aGradient2.Angle &&
            aGradient1.Border == aGradient2.Border &&
            aGradient1.XOffset == aGradient2.XOffset &&
            aGradient1.YOffset == aGradient2.YOffset &&
            aGradient1.StartIntensity == aGradient2.StartIntensity &&
            aGradient1.EndIntensity == aGradient2.EndIntensity &&
            aGradient1.StepCount == aGradient2.StepCount;
}

void DrawingML::WriteGradientFill( Reference< XPropertySet > rXPropSet )
{
    awt::Gradient aGradient;
    if( GETA( FillGradient ) ) {
        aGradient = *static_cast< const awt::Gradient* >( mAny.getValue() );

        // get InteropGrabBag and search the relevant attributes
        awt::Gradient aOriginalGradient;
        Sequence< PropertyValue > aGradientStops;
        if ( GetProperty( rXPropSet, "InteropGrabBag" ) )
        {
            Sequence< PropertyValue > aGrabBag;
            mAny >>= aGrabBag;
            for( sal_Int32 i=0; i < aGrabBag.getLength(); ++i )
                if( aGrabBag[i].Name == "GradFillDefinition" )
                    aGrabBag[i].Value >>= aGradientStops;
                else if( aGrabBag[i].Name == "OriginalGradFill" )
                    aGrabBag[i].Value >>= aOriginalGradient;
        }

        // check if an ooxml gradient had been imported and if the user has modified it
        if( EqualGradients( aOriginalGradient, aGradient ) )
        {
            // If we have no gradient stops that means original gradient were defined by a theme.
            if( aGradientStops.hasElements() )
            {
                mpFS->startElementNS( XML_a, XML_gradFill, FSEND );
                WriteGrabBagGradientFill(aGradientStops, aGradient);
                mpFS->endElementNS( XML_a, XML_gradFill );
            }
        }
        else
        {
            mpFS->startElementNS( XML_a, XML_gradFill, FSEND );
            WriteGradientFill(aGradient);
            mpFS->endElementNS( XML_a, XML_gradFill );
        }
    }
}

void DrawingML::WriteGrabBagGradientFill( const Sequence< PropertyValue >& aGradientStops, awt::Gradient rGradient )
{
    // write back the original gradient
    mpFS->startElementNS( XML_a, XML_gsLst, FSEND );

    // get original stops and write them
    for( sal_Int32 i=0; i < aGradientStops.getLength(); ++i )
    {
        Sequence< PropertyValue > aGradientStop;
        aGradientStops[i].Value >>= aGradientStop;

        // get values
        OUString sSchemeClr;
        double nPos = 0;
        sal_Int16 nTransparency = 0;
        sal_Int32 nRgbClr = 0;
        Sequence< PropertyValue > aTransformations;
        for( sal_Int32 j=0; j < aGradientStop.getLength(); ++j )
            if( aGradientStop[j].Name == "SchemeClr" )
                aGradientStop[j].Value >>= sSchemeClr;
            else if( aGradientStop[j].Name == "RgbClr" )
                aGradientStop[j].Value >>= nRgbClr;
            else if( aGradientStop[j].Name == "Pos" )
                aGradientStop[j].Value >>= nPos;
            else if( aGradientStop[j].Name == "Transparency" )
                aGradientStop[j].Value >>= nTransparency;
            else if( aGradientStop[j].Name == "Transformations" )
                aGradientStop[j].Value >>= aTransformations;

        // write stop
        mpFS->startElementNS( XML_a, XML_gs,
                              XML_pos, OString::number( nPos * 100000.0 ).getStr(),
                              FSEND );
        if( sSchemeClr.isEmpty() )
        {
            // Calculate alpha value (see oox/source/drawingml/color.cxx : getTransparency())
            sal_Int32 nAlpha = (MAX_PERCENT - ( PER_PERCENT * nTransparency ) );
            WriteColor( nRgbClr, nAlpha );
        }
        else
            WriteColor( sSchemeClr, aTransformations );
        mpFS->endElementNS( XML_a, XML_gs );
    }
    mpFS->endElementNS( XML_a, XML_gsLst );

    mpFS->singleElementNS( XML_a, XML_lin,
                           XML_ang, I32S( ( ( ( 3600 - rGradient.Angle + 900 ) * 6000 ) % 21600000 ) ),
                           FSEND );
}

void DrawingML::WriteGradientFill( awt::Gradient rGradient )
{
    switch( rGradient.Style ) {
        default:
        case GradientStyle_LINEAR:
            mpFS->startElementNS( XML_a, XML_gsLst, FSEND );
            WriteGradientStop( 0, ColorWithIntensity( rGradient.StartColor, rGradient.StartIntensity ) );
            WriteGradientStop( 100, ColorWithIntensity( rGradient.EndColor, rGradient.EndIntensity ) );
            mpFS->endElementNS( XML_a, XML_gsLst );
            mpFS->singleElementNS( XML_a, XML_lin,
                    XML_ang, I32S( ( ( ( 3600 - rGradient.Angle + 900 ) * 6000 ) % 21600000 ) ),
                    FSEND );
            break;

        case GradientStyle_AXIAL:
            mpFS->startElementNS( XML_a, XML_gsLst, FSEND );
            WriteGradientStop( 0, ColorWithIntensity( rGradient.EndColor, rGradient.EndIntensity ) );
            WriteGradientStop( 50, ColorWithIntensity( rGradient.StartColor, rGradient.StartIntensity ) );
            WriteGradientStop( 100, ColorWithIntensity( rGradient.EndColor, rGradient.EndIntensity ) );
            mpFS->endElementNS( XML_a, XML_gsLst );
            mpFS->singleElementNS( XML_a, XML_lin,
                    XML_ang, I32S( ( ( ( 3600 - rGradient.Angle + 900 ) * 6000 ) % 21600000 ) ),
                    FSEND );
            break;

            /* I don't see how to apply transformation to gradients, so
             * elliptical will end as radial and square as
             * rectangular. also position offsets are not applied */
        case GradientStyle_RADIAL:
        case GradientStyle_ELLIPTICAL:
        case GradientStyle_RECT:
        case GradientStyle_SQUARE:
            mpFS->startElementNS( XML_a, XML_gsLst, FSEND );
            WriteGradientStop( 0, ColorWithIntensity( rGradient.EndColor, rGradient.EndIntensity ) );
            WriteGradientStop( 100, ColorWithIntensity( rGradient.StartColor, rGradient.StartIntensity ) );
            mpFS->endElementNS( XML_a, XML_gsLst );
            mpFS->singleElementNS( XML_a, XML_path,
                    XML_path, ( rGradient.Style == awt::GradientStyle_RADIAL || rGradient.Style == awt::GradientStyle_ELLIPTICAL ) ? "circle" : "rect",
                    FSEND );
            break;
    }
}

void DrawingML::WriteLineArrow( Reference< XPropertySet > rXPropSet, bool bLineStart )
{
    ESCHER_LineEnd eLineEnd;
    sal_Int32 nArrowLength;
    sal_Int32 nArrowWidth;

    if ( EscherPropertyContainer::GetLineArrow( bLineStart, rXPropSet, eLineEnd, nArrowLength, nArrowWidth ) ) {
        const char* len;
        const char* type;
        const char* width;

        switch( nArrowLength ) {
            case ESCHER_LineShortArrow:
                len = "sm";
                break;
            default:
            case ESCHER_LineMediumLenArrow:
                len = "med";
                break;
            case ESCHER_LineLongArrow:
                len = "lg";
                break;
        }

        switch( eLineEnd ) {
            default:
            case ESCHER_LineNoEnd:
                type = "none";
                break;
            case ESCHER_LineArrowEnd:
                type = "triangle";
                break;
            case ESCHER_LineArrowStealthEnd:
                type = "stealth";
                break;
            case ESCHER_LineArrowDiamondEnd:
                type = "diamond";
                break;
            case ESCHER_LineArrowOvalEnd:
                type = "oval";
                break;
            case ESCHER_LineArrowOpenEnd:
                type = "arrow";
                break;
        }

        switch( nArrowWidth ) {
            case ESCHER_LineNarrowArrow:
                width = "sm";
                break;
            default:
            case ESCHER_LineMediumWidthArrow:
                width = "med";
                break;
            case ESCHER_LineWideArrow:
                width = "lg";
                break;
        }

        mpFS->singleElementNS( XML_a, bLineStart ? XML_headEnd : XML_tailEnd,
                               XML_len, len,
                               XML_type, type,
                               XML_w, width,
                               FSEND );
    }
}

void DrawingML::WriteOutline( Reference< XPropertySet > rXPropSet )
{
    drawing::LineStyle aLineStyle( drawing::LineStyle_NONE );

    GET( aLineStyle, LineStyle );

    sal_uInt32 nLineWidth = 0;
    sal_uInt32 nColor = 0;
    bool bColorSet = false;
    const char* cap = NULL;
    drawing::LineDash aLineDash;
    bool bDashSet = false;
    bool bNoFill = false;

    // get InteropGrabBag and search the relevant attributes
    OUString sColorFillScheme;
    sal_uInt32 nOriginalColor( 0 ), nStyleColor( 0 ), nStyleLineWidth( 0 );
    Sequence< PropertyValue > aStyleProperties, aTransformations;
    drawing::LineStyle aStyleLineStyle( drawing::LineStyle_NONE );
    drawing::LineJoint aStyleLineJoint( drawing::LineJoint_NONE );
    if ( GetProperty( rXPropSet, "InteropGrabBag" ) )
    {
        Sequence< PropertyValue > aGrabBag;
        mAny >>= aGrabBag;
        for( sal_Int32 i=0; i < aGrabBag.getLength(); ++i )
            if( aGrabBag[i].Name == "SpPrLnSolidFillSchemeClr" )
                aGrabBag[i].Value >>= sColorFillScheme;
            else if( aGrabBag[i].Name == "OriginalLnSolidFillClr" )
                aGrabBag[i].Value >>= nOriginalColor;
            else if( aGrabBag[i].Name == "StyleLnRef" )
                aGrabBag[i].Value >>= aStyleProperties;
            else if( aGrabBag[i].Name == "SpPrLnSolidFillSchemeClrTransformations" )
                aGrabBag[i].Value >>= aTransformations;
        if( aStyleProperties.hasElements() )
        {
            for( sal_Int32 i=0; i < aStyleProperties.getLength(); ++i )
                if( aStyleProperties[i].Name == "Color" )
                    aStyleProperties[i].Value >>= nStyleColor;
                else if( aStyleProperties[i].Name == "LineStyle" )
                    aStyleProperties[i].Value >>= aStyleLineStyle;
                else if( aStyleProperties[i].Name == "LineJoint" )
                    aStyleProperties[i].Value >>= aStyleLineJoint;
                else if( aStyleProperties[i].Name == "LineWidth" )
                    aStyleProperties[i].Value >>= nStyleLineWidth;
        }
    }

    GET( nLineWidth, LineWidth );

    switch( aLineStyle ) {
        case drawing::LineStyle_NONE:
            bNoFill = true;
            break;
        case drawing::LineStyle_DASH:
            if( GETA( LineDash ) ) {
                aLineDash = *(drawing::LineDash*) mAny.getValue();
                bDashSet = true;
                if( aLineDash.Style == DashStyle_ROUND || aLineDash.Style == DashStyle_ROUNDRELATIVE )
                    cap = "rnd";

                DBG(fprintf(stderr, "dash dots: %d dashes: %d dotlen: %d dashlen: %d distance: %d\n",
                            int( aLineDash.Dots ), int( aLineDash.Dashes ), int( aLineDash.DotLen ), int( aLineDash.DashLen ), int( aLineDash.Distance )));
            }
            /* fallthru intended */
        case drawing::LineStyle_SOLID:
        default:
            if ( GETA( LineColor ) ) {
                nColor = *((sal_uInt32*) mAny.getValue()) & 0xffffff;
                bColorSet = true;
            }
            break;
    }

    mpFS->startElementNS( XML_a, XML_ln,
                          XML_cap, cap,
                          XML_w, nLineWidth > 1 && nStyleLineWidth != nLineWidth ?
                                  I64S( MM100toEMU( nLineWidth ) ) :NULL,
                          FSEND );

    if( bColorSet )
    {
        if( nColor != nOriginalColor )
            // the user has set a different color for the line
            WriteSolidFill( nColor );
        else if( !sColorFillScheme.isEmpty() )
            // the line had a scheme color and the user didn't change it
            WriteSolidFill( sColorFillScheme, aTransformations );
        else if( aStyleProperties.hasElements() )
        {
            if( nColor != nStyleColor )
                // the line style defines some color but it wasn't being used
                WriteSolidFill( nColor );
            // in case the shape used the style color and the user didn't change it,
            // we must not write a <a: solidFill> tag.
        }
        else
            WriteSolidFill( nColor );
    }

    if( bDashSet && aStyleLineStyle != drawing::LineStyle_DASH ) {
        // line style is a dash and it was not set by the shape style

        if (aLineDash.Dashes == 1 && aLineDash.DashLen == 564 && aLineDash.Distance == 423)
            // That's exactly the predefined "dash" value.
            mpFS->singleElementNS(XML_a, XML_prstDash,
                                  XML_val, "dash",
                                  FSEND);
        else
        {
            mpFS->startElementNS( XML_a, XML_custDash, FSEND );

            // Check that line-width is positive and distance between dashes\dots is positive
            if ( nLineWidth > 0 && aLineDash.Distance > 0 )
            {
                // Write 'dashes' first, and then 'dots'
                int i;
                if ( aLineDash.Dashes > 0 )
                {
                    for( i = 0; i < aLineDash.Dashes; i ++ )
                        mpFS->singleElementNS( XML_a , XML_ds,
                                               XML_d , write1000thOfAPercent( aLineDash.DashLen  > 0 ? aLineDash.DashLen  / nLineWidth * 100 : 100 ),
                                               XML_sp, write1000thOfAPercent( aLineDash.Distance > 0 ? aLineDash.Distance / nLineWidth * 100 : 100 ),
                                               FSEND );
                }
                if ( aLineDash.Dots > 0 )
                {
                    for( i = 0; i < aLineDash.Dots; i ++ )
                        mpFS->singleElementNS( XML_a, XML_ds,
                                               XML_d , write1000thOfAPercent( aLineDash.DotLen   > 0 ? aLineDash.DotLen   / nLineWidth * 100 : 100 ),
                                               XML_sp, write1000thOfAPercent( aLineDash.Distance > 0 ? aLineDash.Distance / nLineWidth * 100 : 100 ),
                                               FSEND );
                }
            }

            if ( nLineWidth <= 0 )
                SAL_WARN("oox", "while writing outline - custom dash - line width was < 0  : " << nLineWidth);
            if ( aLineDash.Dashes < 0 )
                SAL_WARN("oox", "while writing outline - custom dash - number of dashes was < 0  : " << aLineDash.Dashes);
            if ( aLineDash.Dashes > 0 && aLineDash.DashLen <= 0 )
                SAL_WARN("oox", "while writing outline - custom dash - dash length was < 0  : " << aLineDash.DashLen);
            if ( aLineDash.Dots < 0 )
                SAL_WARN("oox", "while writing outline - custom dash - number of dots was < 0  : " << aLineDash.Dots);
            if ( aLineDash.Dots > 0 && aLineDash.DotLen <= 0 )
                SAL_WARN("oox", "while writing outline - custom dash - dot length was < 0  : " << aLineDash.DotLen);
            if ( aLineDash.Distance <= 0 )
                SAL_WARN("oox", "while writing outline - custom dash - distance was < 0  : " << aLineDash.Distance);

            mpFS->endElementNS( XML_a, XML_custDash );
        }
    }

    if( !bNoFill && nLineWidth > 1 && GETA( LineJoint ) ) {
        LineJoint eLineJoint;

        mAny >>= eLineJoint;
        if( aStyleLineJoint == LineJoint_NONE || aStyleLineJoint != eLineJoint )
            // style-defined line joint does not exist, or is different from the shape's joint
            switch( eLineJoint ) {
                case LineJoint_NONE:
                case LineJoint_MIDDLE:
                case LineJoint_BEVEL:
                    mpFS->singleElementNS( XML_a, XML_bevel, FSEND );
                    break;
                default:
                case LineJoint_MITER:
                    mpFS->singleElementNS( XML_a, XML_miter, FSEND );
                    break;
                case LineJoint_ROUND:
                    mpFS->singleElementNS( XML_a, XML_round, FSEND );
                    break;
            }
    }

    if( !bNoFill )
    {
        WriteLineArrow( rXPropSet, true );
        WriteLineArrow( rXPropSet, false );
    }
    else
    {
        mpFS->singleElementNS( XML_a, XML_noFill, FSEND );
    }

    mpFS->endElementNS( XML_a, XML_ln );
}

OUString DrawingML::WriteImage( const OUString& rURL, bool bRelPathToMedia )
{
    OString aURLBS(OUStringToOString(rURL, RTL_TEXTENCODING_UTF8));

    const char aURLBegin[] = "vnd.sun.star.GraphicObject:";
    sal_Int32 index = aURLBS.indexOf(aURLBegin);

    if ( index != -1 )
    {
        DBG(fprintf (stderr, "begin: %ld %s\n", long( sizeof( aURLBegin ) ), USS( rURL ) + RTL_CONSTASCII_LENGTH( aURLBegin ) ));
        Graphic aGraphic = GraphicObject( aURLBS.copy(RTL_CONSTASCII_LENGTH(aURLBegin)) ).GetTransformedGraphic ();

        return WriteImage( aGraphic , bRelPathToMedia );
    } else {
        // add link to relations
    }

    return OUString();
}

const char* DrawingML::GetComponentDir()
{
    switch ( meDocumentType )
    {
        case DOCUMENT_DOCX: return "word";
        case DOCUMENT_PPTX: return "ppt";
        case DOCUMENT_XLSX: return "xl";
    }

    return "unknown";
}

const char* DrawingML::GetRelationCompPrefix()
{
    switch ( meDocumentType )
    {
        case DOCUMENT_DOCX: return "";
        case DOCUMENT_PPTX:
        case DOCUMENT_XLSX: return "../";
    }

    return "unknown";
}

OUString DrawingML::WriteImage( const Graphic& rGraphic , bool bRelPathToMedia )
{
    GfxLink aLink = rGraphic.GetLink ();
    OUString sMediaType;
    const char* pExtension = "";
    OUString sRelId;

    SvMemoryStream aStream;
    const void* aData = aLink.GetData();
    sal_Size nDataSize = aLink.GetDataSize();

    switch ( aLink.GetType() ) {
        case GFX_LINK_TYPE_NATIVE_GIF:
            sMediaType = "image/gif";
            pExtension = ".gif";
            break;

        // #i15508# added BMP type for better exports
        // export not yet active, so adding for reference (not checked)
        case GFX_LINK_TYPE_NATIVE_BMP:
            sMediaType = "image/bmp";
            pExtension = ".bmp";
            break;

        case GFX_LINK_TYPE_NATIVE_JPG:
            sMediaType = "image/jpeg";
            pExtension = ".jpeg";
            break;
        case GFX_LINK_TYPE_NATIVE_PNG:
            sMediaType = "image/png";
            pExtension = ".png";
            break;
        case GFX_LINK_TYPE_NATIVE_TIF:
            sMediaType = "image/tif";
            pExtension = ".tif";
            break;
        case GFX_LINK_TYPE_NATIVE_WMF:
            sMediaType = "image/x-wmf";
            pExtension = ".wmf";
            break;
        case GFX_LINK_TYPE_NATIVE_MET:
            sMediaType = "image/x-met";
            pExtension = ".met";
            break;
        case GFX_LINK_TYPE_NATIVE_PCT:
            sMediaType = "image/x-pict";
            pExtension = ".pct";
            break;
        case GFX_LINK_TYPE_NATIVE_MOV:
            sMediaType = "application/movie";
            pExtension = ".MOV";
            break;
        default: {
            GraphicType aType = rGraphic.GetType();
            if ( aType == GRAPHIC_BITMAP || aType == GRAPHIC_GDIMETAFILE) {
                bool bSwapped = rGraphic.IsSwapOut();

                //Warn rather than just happily swap in because of the comments
                //in the sw export filters about needing to go through the
                //hairy SwGrfNode::SwapIn which we would subvert by swapping in
                //without it knowing about it, so while those ones are fixed we
                //probably have to assume that we should ideally be presented
                //here with already swapped in graphics.
                SAL_WARN_IF(bSwapped, "oox", "attempted to output swapped out graphic");

                if (bSwapped)
                    const_cast<Graphic&>(rGraphic).SwapIn();

                if ( aType == GRAPHIC_BITMAP ) {
                    GraphicConverter::Export( aStream, rGraphic, CVT_PNG );
                    sMediaType = "image/png";
                    pExtension = ".png";
                } else {
                    GraphicConverter::Export( aStream, rGraphic, CVT_EMF );
                    sMediaType = "image/x-emf";
                    pExtension = ".emf";
                }

                if (bSwapped)
                    const_cast<Graphic&>(rGraphic).SwapOut();
            } else {
                OSL_TRACE( "unhandled graphic type" );
                /*Earlier, even in case of unhandled graphic types we were
                  proceeding to write the image, which would eventually
                  write an empty image with a zero size, and return a valid
                  relationID, which is incorrect.
                  */
                return sRelId;
            }

            aData = aStream.GetData();
            nDataSize = aStream.GetEndOfData();
            break;
            }
    }

    Reference< XOutputStream > xOutStream = mpFB->openFragmentStream( OUStringBuffer()
                                                                      .appendAscii( GetComponentDir() )
                                                                      .appendAscii( "/media/image" )
                                                                      .append( (sal_Int32) mnImageCounter )
                                                                      .appendAscii( pExtension )
                                                                      .makeStringAndClear(),
                                                                      sMediaType );
    xOutStream->writeBytes( Sequence< sal_Int8 >( (const sal_Int8*) aData, nDataSize ) );
    xOutStream->closeOutput();

    OString sRelPathToMedia = "media/image";
    if ( bRelPathToMedia )
        sRelPathToMedia = "../" + sRelPathToMedia;
    sRelId = mpFB->addRelation( mpFS->getOutputStream(),
                                "http://schemas.openxmlformats.org/officeDocument/2006/relationships/image",
                                OUStringBuffer()
                                .appendAscii( GetRelationCompPrefix() )
                                .appendAscii( sRelPathToMedia.getStr() )
                                .append( (sal_Int32) mnImageCounter ++ )
                                .appendAscii( pExtension )
                                .makeStringAndClear() );

    return sRelId;
}

OUString DrawingML::WriteBlip( Reference< XPropertySet > rXPropSet, const OUString& rURL, bool bRelPathToMedia, const Graphic *pGraphic )
{
    OUString sRelId = pGraphic ? WriteImage( *pGraphic, bRelPathToMedia ) : WriteImage( rURL, bRelPathToMedia );
    sal_Int16 nBright = 0;
    sal_Int32 nContrast = 0;

    GET( nBright, AdjustLuminance );
    GET( nContrast, AdjustContrast );

    mpFS->startElementNS( XML_a, XML_blip,
            FSNS( XML_r, XML_embed), OUStringToOString( sRelId, RTL_TEXTENCODING_UTF8 ).getStr(),
                  FSEND );
    if( nBright || nContrast )
        mpFS->singleElementNS( XML_a, XML_lum,
                   XML_bright, nBright ? I32S( nBright*1000 ) : NULL,
                   XML_contrast, nContrast ? I32S( nContrast*1000 ) : NULL,
                   FSEND );
    WriteArtisticEffect( rXPropSet );

    mpFS->endElementNS( XML_a, XML_blip );

    return sRelId;
}

void DrawingML::WriteBlipMode( Reference< XPropertySet > rXPropSet, const OUString& rURL )
{
    BitmapMode eBitmapMode( BitmapMode_NO_REPEAT );
    if (GetProperty( rXPropSet, "FillBitmapMode" ) )
        mAny >>= eBitmapMode;

    DBG(fprintf(stderr, "fill bitmap mode: %d\n", eBitmapMode));

    switch (eBitmapMode) {
    case BitmapMode_REPEAT:
        mpFS->singleElementNS( XML_a, XML_tile, FSEND );
        break;
    case BitmapMode_STRETCH:
        WriteStretch( rXPropSet, rURL );
        break;
    default:
        ;
    }
}

void DrawingML::WriteBlipOrNormalFill( Reference< XPropertySet > xPropSet, const OUString& rURLPropName )
{
    // check for blip and otherwise fall back to normal fill
    // we always store normal fill properties but OOXML
    // uses a choice between our fill props and BlipFill
    if (GetProperty ( xPropSet, rURLPropName ))
        WriteBlipFill( xPropSet, rURLPropName );
    else
        WriteFill(xPropSet);
}

void DrawingML::WriteBlipFill( Reference< XPropertySet > rXPropSet, const OUString& sURLPropName )
{
    WriteBlipFill( rXPropSet, sURLPropName, XML_a );
}

void DrawingML::WriteBlipFill( Reference< XPropertySet > rXPropSet, const OUString& sURLPropName, sal_Int32 nXmlNamespace )
{
    if ( GetProperty( rXPropSet, sURLPropName ) ) {
        OUString aURL;
        mAny >>= aURL;
        bool bWriteMode = false;
        if( sURLPropName == "FillBitmapURL" || sURLPropName == "BackGraphicURL")
            bWriteMode = true;
        WriteBlipFill( rXPropSet, aURL, nXmlNamespace, bWriteMode );
    }
}

void DrawingML::WriteBlipFill( Reference< XPropertySet > rXPropSet, const OUString& sBitmapURL, sal_Int32 nXmlNamespace, bool bWriteMode, bool bRelPathToMedia )
{
    if ( !sBitmapURL.isEmpty() ) {
        DBG(fprintf (stderr, "URL: %s\n", OUStringToOString( sBitmapURL, RTL_TEXTENCODING_UTF8 ).getStr() ));

        mpFS->startElementNS( nXmlNamespace , XML_blipFill, FSEND );

        WriteBlip( rXPropSet, sBitmapURL, bRelPathToMedia );

        if( bWriteMode )
            WriteBlipMode( rXPropSet, sBitmapURL );
        else if( GetProperty( rXPropSet, "FillBitmapStretch" ) ) {
                bool bStretch = false;
                mAny >>= bStretch;

                if( bStretch )
                    WriteStretch( rXPropSet, sBitmapURL );
        }
        mpFS->endElementNS( nXmlNamespace, XML_blipFill );
    }
}

void DrawingML::WritePattFill( Reference< XPropertySet > rXPropSet )
{
    if ( GetProperty( rXPropSet, "FillHatch" ) )
    {
        drawing::Hatch aHatch;
        mAny >>= aHatch;

        mpFS->startElementNS( XML_a , XML_pattFill, XML_prst, GetHatchPattern(aHatch), FSEND );

        mpFS->startElementNS( XML_a , XML_fgClr, FSEND );
        WriteColor(aHatch.Color);
        mpFS->endElementNS( XML_a , XML_fgClr );

        // In Writer hatching has no background so use white as a default value.
        mpFS->startElementNS( XML_a , XML_bgClr, FSEND );
        WriteColor(COL_WHITE);
        mpFS->endElementNS( XML_a , XML_bgClr );

        mpFS->endElementNS( XML_a , XML_pattFill );
    }
}

void DrawingML::WriteSrcRect( Reference< XPropertySet > rXPropSet, const OUString& rURL )
{
    Size aOriginalSize( GraphicObject::CreateGraphicObjectFromURL( rURL ).GetPrefSize() );

    if ( GetProperty( rXPropSet, "GraphicCrop" ) )
    {
        ::com::sun::star::text::GraphicCrop aGraphicCropStruct;
        mAny >>= aGraphicCropStruct;

        if ( (0 != aGraphicCropStruct.Left) || (0 != aGraphicCropStruct.Top) || (0 != aGraphicCropStruct.Right) || (0 != aGraphicCropStruct.Bottom) )
        {
            mpFS->singleElementNS( XML_a, XML_srcRect,
                          XML_l, I32S(((aGraphicCropStruct.Left) * 100000)/aOriginalSize.Width()),
                          XML_t, I32S(((aGraphicCropStruct.Top) * 100000)/aOriginalSize.Height()),
                          XML_r, I32S(((aGraphicCropStruct.Right) * 100000)/aOriginalSize.Width()),
                          XML_b, I32S(((aGraphicCropStruct.Bottom) * 100000)/aOriginalSize.Height()),
                          FSEND );
        }
    }
}

void DrawingML::WriteStretch( ::com::sun::star::uno::Reference< ::com::sun::star::beans::XPropertySet > rXPropSet, const OUString& rURL )
{
    mpFS->startElementNS( XML_a, XML_stretch, FSEND );

    bool bCrop = false;
    if ( GetProperty( rXPropSet, "GraphicCrop" ) )
    {
        ::com::sun::star::text::GraphicCrop aGraphicCropStruct;
        mAny >>= aGraphicCropStruct;

        if ( (0 != aGraphicCropStruct.Left) || (0 != aGraphicCropStruct.Top) || (0 != aGraphicCropStruct.Right) || (0 != aGraphicCropStruct.Bottom) )
        {
            Size aOriginalSize( GraphicObject::CreateGraphicObjectFromURL( rURL ).GetPrefSize() );
            mpFS->singleElementNS( XML_a, XML_fillRect,
                          XML_l, I32S(((aGraphicCropStruct.Left) * 100000)/aOriginalSize.Width()),
                          XML_t, I32S(((aGraphicCropStruct.Top) * 100000)/aOriginalSize.Height()),
                          XML_r, I32S(((aGraphicCropStruct.Right) * 100000)/aOriginalSize.Width()),
                          XML_b, I32S(((aGraphicCropStruct.Bottom) * 100000)/aOriginalSize.Height()),
                          FSEND );
            bCrop = true;
        }
    }

    if( !bCrop )
    {
        mpFS->singleElementNS( XML_a, XML_fillRect, FSEND );
    }

    mpFS->endElementNS( XML_a, XML_stretch );
}

void DrawingML::WriteTransformation( const Rectangle& rRect,
        sal_Int32 nXmlNamespace, bool bFlipH, bool bFlipV, sal_Int32 nRotation )
{
    mpFS->startElementNS( nXmlNamespace, XML_xfrm,
                          XML_flipH, bFlipH ? "1" : NULL,
                          XML_flipV, bFlipV ? "1" : NULL,
                          XML_rot, (nRotation % 21600000) ? I32S( nRotation ) : NULL,
                          FSEND );

    sal_Int32 nLeft = rRect.Left();
    sal_Int32 nTop = rRect.Top();
    if (GetDocumentType() == DOCUMENT_DOCX && !m_xParent.is())
    {
        nLeft = 0;
        nTop = 0;
    }

    mpFS->singleElementNS( XML_a, XML_off, XML_x, IS( MM100toEMU( nLeft ) ), XML_y, IS( MM100toEMU( nTop ) ), FSEND );
    mpFS->singleElementNS( XML_a, XML_ext, XML_cx, IS( MM100toEMU( rRect.GetWidth() ) ), XML_cy, IS( MM100toEMU( rRect.GetHeight() ) ), FSEND );

    mpFS->endElementNS( nXmlNamespace, XML_xfrm );
}

void DrawingML::WriteShapeTransformation( Reference< XShape > rXShape, sal_Int32 nXmlNamespace, bool bFlipH, bool bFlipV, bool bSuppressRotation  )
{
    DBG(fprintf(stderr,  "write shape transformation\n" ));

    sal_Int32 nRotation=0;
    awt::Point aPos = rXShape->getPosition();
    awt::Size aSize = rXShape->getSize();

    if (m_xParent.is())
    {
        awt::Point aParentPos = m_xParent->getPosition();
        aPos.X -= aParentPos.X;
        aPos.Y -= aParentPos.Y;
    }

    if ( aSize.Width < 0 )
        aSize.Width = 1000;
    if ( aSize.Height < 0 )
        aSize.Height = 1000;
    if (!bSuppressRotation)
    {
        SdrObject* pShape = (SdrObject*) GetSdrObjectFromXShape( rXShape );
        nRotation = pShape ? pShape->GetRotateAngle() : 0;
        if (nRotation != 0 && nRotation != 18000)
        {
            int faccos=bFlipV ? -1 : 1;
            int facsin=bFlipH ? -1 : 1;
            aPos.X-=(1-faccos*cos(nRotation*F_PI18000))*aSize.Width/2-facsin*sin(nRotation*F_PI18000)*aSize.Height/2;
            aPos.Y-=(1-faccos*cos(nRotation*F_PI18000))*aSize.Height/2+facsin*sin(nRotation*F_PI18000)*aSize.Width/2;
        }
    }
    if (!bSuppressRotation)
    {
        if (bFlipV) {nRotation=(nRotation+18000)%36000;}
    }
    WriteTransformation( Rectangle( Point( aPos.X, aPos.Y ), Size( aSize.Width, aSize.Height ) ), nXmlNamespace, bFlipH, bFlipV, OOX_DRAWINGML_EXPORT_ROTATE_CLOCKWISIFY(nRotation) );
}

void DrawingML::WriteRunProperties( Reference< XPropertySet > rRun, bool bIsField )
{
    Reference< XPropertySet > rXPropSet( rRun, UNO_QUERY );
    Reference< XPropertyState > rXPropState( rRun, UNO_QUERY );
    OUString usLanguage;
    PropertyState eState;
    sal_Int16 nScriptType = SvtLanguageOptions::GetScriptTypeOfLanguage( Application::GetSettings().GetLanguageTag().getLanguageType() );
    bool bComplex = ( nScriptType == ScriptType::COMPLEX );
    const char* bold = NULL;
    const char* italic = NULL;
    const char* underline = NULL;
    const char* strikeout = NULL;
    sal_Int32 nSize = 1800;
    sal_Int32 nCharEscapement = 0;

    if( GETA( CharHeight ) )
        nSize = (sal_Int32) (100*(*((float*) mAny.getValue())));

    if ( ( bComplex && GETA( CharWeightComplex ) ) || GETA( CharWeight ) )
        if ( *((float*) mAny.getValue()) >= awt::FontWeight::SEMIBOLD )
            bold = "1";

    if ( ( bComplex && GETA( CharPostureComplex ) ) || GETA( CharPosture ) )
        switch ( *((awt::FontSlant*) mAny.getValue()) )
        {
            case awt::FontSlant_OBLIQUE :
            case awt::FontSlant_ITALIC :
                italic = "1";
                break;
            default:
                break;
        }

    if ( GETAD( CharUnderline ) )
        switch ( *((sal_Int16*) mAny.getValue()) )
        {
            case awt::FontUnderline::SINGLE :
                underline = "sng";
                break;
            case awt::FontUnderline::DOUBLE :
                underline = "dbl";
                break;
            case awt::FontUnderline::DOTTED :
                underline = "dotted";
                break;
            case awt::FontUnderline::DASH :
                underline = "dash";
                break;
            case awt::FontUnderline::LONGDASH :
                underline = "dashLong";
                break;
            case awt::FontUnderline::DASHDOT :
                underline = "dotDash";
                break;
            case awt::FontUnderline::DASHDOTDOT :
                underline = "dotDotDash";
                break;
            case awt::FontUnderline::WAVE :
                underline = "wavy";
                break;
            case awt::FontUnderline::DOUBLEWAVE :
                underline = "wavyDbl";
                break;
            case awt::FontUnderline::BOLD :
                underline = "heavy";
                break;
            case awt::FontUnderline::BOLDDOTTED :
                underline = "dottedHeavy";
                break;
            case awt::FontUnderline::BOLDDASH :
                underline = "dashHeavy";
                break;
            case awt::FontUnderline::BOLDLONGDASH :
                underline = "dashLongHeavy";
                break;
            case awt::FontUnderline::BOLDDASHDOT :
                underline = "dotDashHeavy";
                break;
            case awt::FontUnderline::BOLDDASHDOTDOT :
                underline = "dotDotDashHeavy";
                break;
            case awt::FontUnderline::BOLDWAVE :
                underline = "wavyHeavy";
                break;
        }

    if ( GETAD( CharStrikeout ) )
    {
        switch ( *((sal_Int16*) mAny.getValue()) )
        {
            case awt::FontStrikeout::NONE :
               strikeout = "noStrike";
               break;
            case awt::FontStrikeout::SINGLE :
            // LibO supports further values of character
            // strikeout, OOXML standard (20.1.10.78,
            // ST_TextStrikeType) however specifies only
            // 3 - single, double and none. Approximate
            // the remaining ones by single strike (better
            // some strike than none at all).
            // TODO: figure out how to do this better
            case awt::FontStrikeout::BOLD :
            case awt::FontStrikeout::SLASH :
            case awt::FontStrikeout::X :
               strikeout = "sngStrike";
               break;
            case awt::FontStrikeout::DOUBLE :
               strikeout = "dblStrike";
               break;
        }
    }

    if( GETA( CharLocale ) ) {
        com::sun::star::lang::Locale aLocale;
        mAny >>= aLocale;
        LanguageTag aLanguageTag( aLocale);
        if (!aLanguageTag.isSystemLocale())
            usLanguage = aLanguageTag.getBcp47();
    }

    if( GETAD( CharEscapement ) )
        mAny >>= nCharEscapement;

    if( nCharEscapement && GETAD( CharEscapementHeight ) ) {
        sal_uInt32 nCharEscapementHeight = 0;
        mAny >>= nCharEscapementHeight;
        nSize = (nSize * nCharEscapementHeight) / 100;
        // MSO uses default ~58% size
        nSize = (nSize / 0.58);
    }

    mpFS->startElementNS( XML_a, XML_rPr,
                          XML_b, bold,
                          XML_i, italic,
                          XML_lang, usLanguage.isEmpty() ? NULL : USS( usLanguage ),
                          XML_sz, nSize == 1800 ? NULL : IS( nSize ),
                          XML_strike, strikeout,
                          XML_u, underline,
                          XML_baseline, nCharEscapement == 0 ? NULL : IS( nCharEscapement*1000 ),
                          FSEND );

    // mso doesn't like text color to be placed after typeface
    if( GETAD( CharColor ) ) {
        sal_uInt32 color = *((sal_uInt32*) mAny.getValue());
        DBG(fprintf(stderr, "run color: %x auto: %x\n", static_cast<unsigned int>( color ), static_cast<unsigned int>( COL_AUTO )));

        if( color == COL_AUTO ) { // nCharColor depends to the background color
            bool bIsDark = false;
            GET( bIsDark, IsBackgroundDark );
            color = bIsDark ? 0xffffff : 0x000000;
        }
        color &= 0xffffff;

        // TODO: special handle embossed/engraved

        WriteSolidFill( color );
    }

    if( GETA( CharFontName ) ) {
        const char* pitch = NULL;
        const char* charset = NULL;
        OUString usTypeface;

        mAny >>= usTypeface;
        OUString aSubstName( GetSubsFontName( usTypeface, SUBSFONT_ONLYONE | SUBSFONT_MS ) );

        mpFS->singleElementNS( XML_a, XML_latin,
                               XML_typeface, USS(aSubstName.getLength() ? aSubstName : usTypeface),
                               XML_pitchFamily, pitch,
                               XML_charset, charset,
                               FSEND );
    }

    if( ( bComplex && GETAD( CharFontNameComplex ) ) || ( !bComplex && GETAD( CharFontNameAsian ) ) ) {
        const char* pitch = NULL;
        const char* charset = NULL;
        OUString usTypeface;

        mAny >>= usTypeface;
        OUString aSubstName( GetSubsFontName( usTypeface, SUBSFONT_ONLYONE | SUBSFONT_MS ) );

        mpFS->singleElementNS( XML_a, bComplex ? XML_cs : XML_ea,
                               XML_typeface, USS(aSubstName.getLength() ? aSubstName : usTypeface),
                               XML_pitchFamily, pitch,
                               XML_charset, charset,
                               FSEND );
    }

    if( bIsField ) {
        Reference< XTextField > rXTextField;
        GET( rXTextField, TextField );
        if( rXTextField.is() )
            rRun.set( rXTextField, UNO_QUERY );
    }

    // field properties starts here
    if( GETA( URL ) ) {
    OUString sURL;

    mAny >>= sURL;
    if( !sURL.isEmpty() ) {
        OUString sRelId = mpFB->addRelation( mpFS->getOutputStream(),
                              "http://schemas.openxmlformats.org/officeDocument/2006/relationships/hyperlink",
                              sURL, true );

        mpFS->singleElementNS( XML_a, XML_hlinkClick,
                   FSNS( XML_r,XML_id ), USS( sRelId ),
                   FSEND );
    }
    }

    mpFS->endElementNS( XML_a, XML_rPr );
}

const char* DrawingML::GetFieldType( ::com::sun::star::uno::Reference< ::com::sun::star::text::XTextRange > rRun, bool& bIsField )
{
    const char* sType = NULL;
    Reference< XPropertySet > rXPropSet( rRun, UNO_QUERY );
    OUString aFieldType;

    if( GETA( TextPortionType ) ) {
        aFieldType = OUString( *(OUString*)mAny.getValue() );
        DBG(fprintf (stderr, "field type: %s\n", USS(aFieldType) ));
    }

    if( aFieldType == "TextField" ) {
        Reference< XTextField > rXTextField;
        GET( rXTextField, TextField );
        if( rXTextField.is() ) {
        bIsField = true;
            rXPropSet.set( rXTextField, UNO_QUERY );
            if( rXPropSet.is() ) {
                OUString aFieldKind( rXTextField->getPresentation( true ) );
                DBG(fprintf (stderr, "field kind: %s\n", USS(aFieldKind) ));
                if( aFieldKind == "Page" ) {
                    return "slidenum";
                }
        // else if( aFieldKind == "URL" ) {
        // do not return here
        // and make URL field text run with hyperlink property later
        // }
            }
        }
    }

    return sType;
}

void DrawingML::GetUUID( OStringBuffer& rBuffer )
{
    Sequence< sal_uInt8 > aSeq( 16 );
    static const char cDigits[17] = "0123456789ABCDEF";
    rtl_createUuid( (sal_uInt8*)aSeq.getArray(), 0, true );
    int i;

    rBuffer.append( '{' );
    for( i = 0; i < 4; i++ ) {
        rBuffer.append( cDigits[ aSeq[i] >> 4 ] );
        rBuffer.append( cDigits[ aSeq[i] & 0xf ] );
    }
    rBuffer.append( '-' );
    for( ; i < 6; i++ ) {
        rBuffer.append( cDigits[ aSeq[i] >> 4 ] );
        rBuffer.append( cDigits[ aSeq[i] & 0xf ] );
    }
    rBuffer.append( '-' );
    for( ; i < 8; i++ ) {
        rBuffer.append( cDigits[ aSeq[i] >> 4 ] );
        rBuffer.append( cDigits[ aSeq[i] & 0xf ] );
    }
    rBuffer.append( '-' );
    for( ; i < 10; i++ ) {
        rBuffer.append( cDigits[ aSeq[i] >> 4 ] );
        rBuffer.append( cDigits[ aSeq[i] & 0xf ] );
    }
    rBuffer.append( '-' );
    for( ; i < 16; i++ ) {
        rBuffer.append( cDigits[ aSeq[i] >> 4 ] );
        rBuffer.append( cDigits[ aSeq[i] & 0xf ] );
    }
    rBuffer.append( '}' );
}

void DrawingML::WriteRun( Reference< XTextRange > rRun )
{
    const char* sFieldType;
    bool bIsField = false;
    OUString sText = rRun->getString();

    if( sText.isEmpty()) {
        Reference< XPropertySet > xPropSet( rRun, UNO_QUERY );

        try {
        if( !xPropSet.is() || !( xPropSet->getPropertyValue( "PlaceholderText" ) >>= sText ) )
            return;
        if( sText.isEmpty() )
            return;
        }
        catch (const Exception &) {
            return;
        }
    }

    sFieldType = GetFieldType( rRun, bIsField );
    if( ( sFieldType != NULL ) ) {
        OStringBuffer sUUID(39);

        GetUUID( sUUID );
        mpFS->startElementNS( XML_a, XML_fld,
                              XML_id, sUUID.getStr(),
                              XML_type, sFieldType,
                              FSEND );
    } else
        mpFS->startElementNS( XML_a, XML_r, FSEND );

    Reference< XPropertySet > xPropSet( rRun, uno::UNO_QUERY );
    WriteRunProperties( xPropSet, bIsField );

    mpFS->startElementNS( XML_a, XML_t, FSEND );
    mpFS->writeEscaped( sText );
    mpFS->endElementNS( XML_a, XML_t );

    if( sFieldType )
        mpFS->endElementNS( XML_a, XML_fld );
    else
        mpFS->endElementNS( XML_a, XML_r );
}

#define AUTONUM(x) \
                        if( bPBoth ) \
                            pAutoNumType = #x "ParenBoth"; \
                        else if( bPBehind ) \
                            pAutoNumType = #x "ParenR"; \
                        else if( bSDot ) \
                            pAutoNumType = #x "Period";

inline static const char* GetAutoNumType( sal_Int16 nNumberingType, bool bSDot, bool bPBehind, bool bPBoth )
{
    const char* pAutoNumType = NULL;

    switch( (SvxExtNumType)nNumberingType )
        {
        case SVX_NUM_CHARS_UPPER_LETTER_N :
        case SVX_NUM_CHARS_UPPER_LETTER :
            AUTONUM( alphaUc );
            break;
        case SVX_NUM_CHARS_LOWER_LETTER_N :
        case SVX_NUM_CHARS_LOWER_LETTER :
            AUTONUM( alphaLc );
            break;
        case SVX_NUM_ROMAN_UPPER :
            AUTONUM( romanUc );
            break;
        case SVX_NUM_ROMAN_LOWER :
            AUTONUM( romanLc );
            break;
        case SVX_NUM_ARABIC :
            AUTONUM( arabic )
            else
                pAutoNumType = "arabicPlain";
                        break;
        default:
            break;
        }

    return pAutoNumType;
}

void DrawingML::WriteParagraphNumbering( Reference< XPropertySet > rXPropSet, sal_Int16 nLevel )
{
    if( nLevel >= 0 && GETA( NumberingRules ) )
    {
        Reference< XIndexAccess > rXIndexAccess;

        if ( ( mAny >>= rXIndexAccess ) && nLevel < rXIndexAccess->getCount() )
        {
            DBG(fprintf (stderr, "numbering rules\n"));

            Sequence< PropertyValue > aPropertySequence;
            rXIndexAccess->getByIndex( nLevel ) >>= aPropertySequence;

            const PropertyValue* pPropValue = aPropertySequence.getArray();

            sal_Int32 nPropertyCount = aPropertySequence.getLength();

            if ( nPropertyCount ) {

                sal_Int16 nNumberingType = -1;
                bool bSDot = false;
                bool bPBehind = false;
                bool bPBoth = false;
                sal_Unicode aBulletChar = 0x2022; // a bullet
                awt::FontDescriptor aFontDesc;
                bool bHasFontDesc = false;
                OUString aGraphicURL;
                sal_Int16 nBulletRelSize = 0;

                for ( sal_Int32 i = 0; i < nPropertyCount; i++ ) {
                    const void* pValue = pPropValue[ i ].Value.getValue();
                    if ( pValue ) {
                        OUString aPropName( pPropValue[ i ].Name );
                        DBG(fprintf (stderr, "pro name: %s\n", OUStringToOString( aPropName, RTL_TEXTENCODING_UTF8 ).getStr()));
                        if ( aPropName == "NumberingType" )
                            nNumberingType = *( (sal_Int16*)pValue );
                        else if ( aPropName == "Prefix" ) {
                            if( *(OUString*)pValue == ")")
                                bPBoth = true;
                        } else if ( aPropName == "Suffix" ) {
                            if( *(OUString*)pValue == ".")
                                bSDot = true;
                            else if( *(OUString*)pValue == ")")
                                bPBehind = true;
                        } else if ( aPropName == "BulletChar" )
                        {
                            aBulletChar = OUString ( *( (OUString*)pValue ) )[ 0 ];
                            //printf ("bullet char: %d\n", aBulletChar.getStr());
                        }
                        else if ( aPropName == "BulletFont" )
                        {
                            aFontDesc = *( (awt::FontDescriptor*)pValue );
                            bHasFontDesc = true;

                            // Our numbullet dialog has set the wrong textencoding for our "StarSymbol" font,
                            // instead of a Unicode encoding the encoding RTL_TEXTENCODING_SYMBOL was used.
                            // Because there might exist a lot of damaged documemts I added this two lines
                            // which fixes the bullet problem for the export.
                            if ( aFontDesc.Name.equalsIgnoreAsciiCase("StarSymbol") )
                                aFontDesc.CharSet = RTL_TEXTENCODING_MS_1252;

                        } else if ( aPropName == "BulletRelSize" ) {
                            nBulletRelSize = *( (sal_Int16*)pValue );
                        } else if ( aPropName == "GraphicURL" ) {
                            aGraphicURL = ( *(OUString*)pValue );
                            DBG(fprintf (stderr, "graphic url: %s\n", OUStringToOString( aGraphicURL, RTL_TEXTENCODING_UTF8 ).getStr()));
                        } else if ( aPropName == "GraphicSize" )
                        {
                            if ( pPropValue[ i ].Value.getValueType() == cppu::UnoType<awt::Size>::get())
                            {
                                // don't cast awt::Size to Size as on 64-bits they are not the same.
                                ::com::sun::star::awt::Size aSize;
                                pPropValue[ i ].Value >>= aSize;
                                //aBuGraSize.nA = aSize.Width;
                                //aBuGraSize.nB = aSize.Height;
                                DBG(fprintf(stderr, "graphic size: %dx%d\n", int( aSize.Width ), int( aSize.Height )));
                            }
                        }
                    }
                }

                const char* pAutoNumType = GetAutoNumType( nNumberingType, bSDot, bPBehind, bPBoth );

                if( nLevel >= 0 ) {
                    if( !aGraphicURL.isEmpty() ) {
                        OUString sRelId = WriteImage( aGraphicURL );

                        mpFS->startElementNS( XML_a, XML_buBlip, FSEND );
                        mpFS->singleElementNS( XML_a, XML_blip, FSNS( XML_r, XML_embed ), USS( sRelId ), FSEND );
                        mpFS->endElementNS( XML_a, XML_buBlip );
                    } else {
                        if( nBulletRelSize && nBulletRelSize != 100 )
                            mpFS->singleElementNS( XML_a, XML_buSzPct,
                                                   XML_val, IS( std::max( (sal_Int32)25000, std::min( (sal_Int32)400000, 1000*( (sal_Int32)nBulletRelSize ) ) ) ), FSEND );
                        if( bHasFontDesc )
                            mpFS->singleElementNS( XML_a, XML_buFont,
                                                   XML_typeface, OUStringToOString( aFontDesc.Name, RTL_TEXTENCODING_UTF8 ).getStr(),
                                                   XML_charset, (aFontDesc.CharSet == awt::CharSet::SYMBOL) ? "2" : NULL,
                                                   FSEND );

                        if( pAutoNumType )
                            mpFS->singleElementNS( XML_a, XML_buAutoNum, XML_type, pAutoNumType, FSEND );
                        else {
                            aBulletChar = SubstituteBullet( aBulletChar, aFontDesc );
                            mpFS->singleElementNS( XML_a, XML_buChar, XML_char, USS( OUString( aBulletChar ) ), FSEND );
                        }
                    }
                }
            }
        }
    }
}

const char* DrawingML::GetAlignment( sal_Int32 nAlignment )
{
    const char* sAlignment = NULL;

    switch( nAlignment ) {
        case style::ParagraphAdjust_CENTER:
            sAlignment = "ctr";
            break;
        case style::ParagraphAdjust_RIGHT:
            sAlignment = "r";
            break;
        case style::ParagraphAdjust_BLOCK:
            sAlignment = "just";
            break;
        default:
            ;
    }

    return sAlignment;
}

void DrawingML::WriteLinespacing( LineSpacing& rSpacing )
{
    if( rSpacing.Mode == LineSpacingMode::PROP )
        mpFS->singleElementNS( XML_a, XML_spcPct,
                               XML_val, I32S( ((sal_Int32)rSpacing.Height)*1000 ),
                               FSEND );
    else
        mpFS->singleElementNS( XML_a, XML_spcPts,
                               XML_val, I32S( rSpacing.Height ),
                               FSEND );
}

void DrawingML::WriteParagraphProperties( Reference< XTextContent > rParagraph )
{
    Reference< XPropertySet > rXPropSet( rParagraph, UNO_QUERY );
    Reference< XPropertyState > rXPropState( rParagraph, UNO_QUERY );
    PropertyState eState;

    if( !rXPropSet.is() || !rXPropState.is() )
        return;

    sal_Int16 nLevel = -1;
    GET( nLevel, NumberingLevel );

    sal_Int16 nAlignment( style::ParagraphAdjust_LEFT );
    GET( nAlignment, ParaAdjust );

    bool bHasLinespacing = false;
    LineSpacing aLineSpacing;
    if( GETAD( ParaLineSpacing ) )
        bHasLinespacing = ( mAny >>= aLineSpacing );

    if( nLevel != -1
        || nAlignment != style::ParagraphAdjust_LEFT
        || bHasLinespacing ) {
        mpFS->startElementNS( XML_a, XML_pPr,
                              XML_lvl, nLevel > 0 ? I32S( nLevel ) : NULL,
                              XML_marL, NULL,
                              XML_algn, GetAlignment( nAlignment ),
                              FSEND );

        if( bHasLinespacing ) {
            mpFS->startElementNS( XML_a, XML_lnSpc, FSEND );
            WriteLinespacing( aLineSpacing );
            mpFS->endElementNS( XML_a, XML_lnSpc );
        }

        WriteParagraphNumbering( rXPropSet, nLevel );

        mpFS->endElementNS( XML_a, XML_pPr );
    }
}

void DrawingML::WriteParagraph( Reference< XTextContent > rParagraph )
{
    Reference< XEnumerationAccess > access( rParagraph, UNO_QUERY );
    if( !access.is() )
        return;

    Reference< XEnumeration > enumeration( access->createEnumeration() );
    if( !enumeration.is() )
        return;

    mpFS->startElementNS( XML_a, XML_p, FSEND );

    bool bPropertiesWritten = false;
    while( enumeration->hasMoreElements() ) {
        Reference< XTextRange > run;
        Any any ( enumeration->nextElement() );

        if (any >>= run) {
            if( !bPropertiesWritten ) {
                WriteParagraphProperties( rParagraph );
                bPropertiesWritten = true;
            }
            WriteRun( run );
        }
    }
    mpFS->singleElementNS( XML_a, XML_endParaRPr, FSEND );

    mpFS->endElementNS( XML_a, XML_p );
}

void DrawingML::WriteText( Reference< XInterface > rXIface, const OUString& presetWarp, bool bBodyPr, bool bText, sal_Int32 nXmlNamespace )
{
    Reference< XText > xXText( rXIface, UNO_QUERY );
    Reference< XPropertySet > rXPropSet( rXIface, UNO_QUERY );

    if( !xXText.is() )
        return;

#define DEFLRINS 254
#define DEFTBINS 127
    sal_Int32 nLeft, nRight, nTop, nBottom;
    nLeft = nRight = DEFLRINS;
    nTop = nBottom = DEFTBINS;

    // top inset looks a bit different compared to ppt export
    // check if something related doesn't work as expected
    GET( nLeft, TextLeftDistance );
    GET( nRight, TextRightDistance );
    GET( nTop, TextUpperDistance );
    GET( nBottom, TextLowerDistance );

    TextVerticalAdjust eVerticalAlignment( TextVerticalAdjust_TOP );
    const char* sVerticalAlignment = NULL;
    GET( eVerticalAlignment, TextVerticalAdjust );
    if( eVerticalAlignment != TextVerticalAdjust_TOP )
        sVerticalAlignment = GetTextVerticalAdjust(eVerticalAlignment);

    const char* sWritingMode = NULL;
    bool bVertical = false;
    if( GETA( TextWritingMode ) ) {
        WritingMode eMode;

        if( ( mAny >>= eMode ) && eMode == WritingMode_TB_RL ) {
            sWritingMode = "vert";
            bVertical = true;
        }
    }

    if ( GETA( CustomShapeGeometry ) )
    {
        Sequence< PropertyValue > aProps;
        if ( mAny >>= aProps )
        {
            for ( sal_Int32 i = 0, nElems = aProps.getLength(); i < nElems; ++i )
            {
                sal_Int32 nTextRotateAngle = 0;
                if ( aProps[ i ].Name.equals( "TextPreRotateAngle" ) && ( aProps[ i ].Value >>= nTextRotateAngle ) )
                {
                    if ( nTextRotateAngle == -90 )
                    {
                        sWritingMode = "vert";
                        bVertical = true;
                    }
                    else if ( nTextRotateAngle == -270 )
                    {
                        sWritingMode = "vert270";
                        bVertical = true;
                    }
                    break;
                }
            }
        }
    }

    TextHorizontalAdjust eHorizontalAlignment( TextHorizontalAdjust_CENTER );
    bool bHorizontalCenter = false;
    GET( eHorizontalAlignment, TextHorizontalAdjust );
    if( eHorizontalAlignment == TextHorizontalAdjust_CENTER )
        bHorizontalCenter = true;
    else if( bVertical && eHorizontalAlignment == TextHorizontalAdjust_LEFT )
        sVerticalAlignment = "b";

    bool bHasWrap = false;
    bool bWrap = false;
    // Only custom shapes obey the TextWordWrap option, normal text always wraps.
    if( dynamic_cast<SvxCustomShape*>(rXIface.get()) && GETA( TextWordWrap ) ) {
        mAny >>= bWrap;
        bHasWrap = true;
    }

    if (bBodyPr)
    {
        const char* pWrap = bHasWrap && !bWrap ? "none" : NULL;
        if (GetDocumentType() == DOCUMENT_DOCX)
        {
            // In case of DOCX, if we want to have the same effect as
            // TextShape's automatic word wrapping, then we need to set
            // wrapping to square.
            uno::Reference<lang::XServiceInfo> xServiceInfo(rXIface, uno::UNO_QUERY);
            if (xServiceInfo.is() && xServiceInfo->supportsService("com.sun.star.drawing.TextShape"))
                pWrap = "square";
        }
        mpFS->startElementNS( (nXmlNamespace ? nXmlNamespace : XML_a), XML_bodyPr,
                               XML_wrap, pWrap,
                               XML_lIns, (nLeft != DEFLRINS) ? IS( MM100toEMU( nLeft ) ) : NULL,
                               XML_rIns, (nRight != DEFLRINS) ? IS( MM100toEMU( nRight ) ) : NULL,
                               XML_tIns, (nTop != DEFTBINS) ? IS( MM100toEMU( nTop ) ) : NULL,
                               XML_bIns, (nBottom != DEFTBINS) ? IS( MM100toEMU( nBottom ) ) : NULL,
                               XML_anchor, sVerticalAlignment,
                               XML_anchorCtr, bHorizontalCenter ? "1" : NULL,
                               XML_vert, sWritingMode,
                               FSEND );
        if( presetWarp != NULL  && !presetWarp.isEmpty())
        {
            mpFS->singleElementNS(XML_a, XML_prstTxWarp, XML_prst,OUStringToOString(presetWarp, RTL_TEXTENCODING_UTF8 ).getStr(),
                FSEND );
        }
        if (GetDocumentType() == DOCUMENT_DOCX)
        {
            sal_Bool bTextAutoGrowHeight = sal_False;
            GET(bTextAutoGrowHeight, TextAutoGrowHeight);
            mpFS->singleElementNS(XML_a, (bTextAutoGrowHeight ? XML_spAutoFit : XML_noAutofit), FSEND);
        }
        mpFS->endElementNS((nXmlNamespace ? nXmlNamespace : XML_a), XML_bodyPr);
    }

    Reference< XEnumerationAccess > access( xXText, UNO_QUERY );
    if( !access.is() || !bText )
        return;

    Reference< XEnumeration > enumeration( access->createEnumeration() );
    if( !enumeration.is() )
        return;

    SdrObject* pSdrObject = GetSdrObjectFromXShape(uno::Reference<drawing::XShape>(rXIface, uno::UNO_QUERY_THROW));
    const SdrTextObj* pTxtObj = PTR_CAST(SdrTextObj, pSdrObject);
    if (pTxtObj && mpTextExport)
    {
        const OutlinerParaObject* pParaObj = 0;
        bool bOwnParaObj = false;

        /*
        #i13885#
        When the object is actively being edited, that text is not set into
        the objects normal text object, but lives in a separate object.
        */
        if (pTxtObj->IsTextEditActive())
        {
            pParaObj = pTxtObj->GetEditOutlinerParaObject();
            bOwnParaObj = true;
        }
        else
            pParaObj = pTxtObj->GetOutlinerParaObject();

        if (pParaObj)
        {
            // this is reached only in case some text is attached to the shape
            mpTextExport->WriteOutliner(*pParaObj);
            if (bOwnParaObj)
                delete pParaObj;
        }
        return;
    }

    while( enumeration->hasMoreElements() ) {
        Reference< XTextContent > paragraph;
        Any any ( enumeration->nextElement() );

        if( any >>= paragraph)
            WriteParagraph( paragraph );
    }

}

void DrawingML::WritePresetShape( const char* pShape )
{
    mpFS->startElementNS( XML_a, XML_prstGeom,
                          XML_prst, pShape,
                          FSEND );
    mpFS->singleElementNS( XML_a, XML_avLst, FSEND );
    mpFS->endElementNS(  XML_a, XML_prstGeom );
}

std::map< OString, std::vector<OString> > lcl_getAdjNames()
{
    std::map< OString, std::vector<OString> > aRet;

    OUString aPath("$BRAND_BASE_DIR/" LIBO_SHARE_FOLDER "/filter/oox-drawingml-adj-names");
    rtl::Bootstrap::expandMacros(aPath);
    SvFileStream aStream(aPath, STREAM_READ);
    if (aStream.GetError() != ERRCODE_NONE)
        SAL_WARN("oox", "failed to open oox-drawingml-adj-names");
    OString aLine;
    bool bNotDone = aStream.ReadLine(aLine);
    while (bNotDone)
    {
        sal_Int32 nIndex = 0;
        // Each line is in a "key\tvalue" format: read the key, the rest is the value.
        OString aKey = aLine.getToken(0, '\t', nIndex);
        OString aValue = aLine.copy(nIndex);
        aRet[aKey].push_back(aValue);
        bNotDone = aStream.ReadLine(aLine);
    }
    return aRet;
}

void DrawingML::WritePresetShape( const char* pShape, MSO_SPT eShapeType, bool bPredefinedHandlesUsed, sal_Int32 nAdjustmentsWhichNeedsToBeConverted, const PropertyValue& rProp )
{
    static std::map< OString, std::vector<OString> > aAdjMap = lcl_getAdjNames();
    // If there are predefined adj names for this shape type, look them up now.
    std::vector<OString> aAdjustments;
    if (aAdjMap.find(OString(pShape)) != aAdjMap.end())
        aAdjustments = aAdjMap[OString(pShape)];

    mpFS->startElementNS( XML_a, XML_prstGeom,
                          XML_prst, pShape,
                          FSEND );
    mpFS->startElementNS( XML_a, XML_avLst, FSEND );

    Sequence< drawing::EnhancedCustomShapeAdjustmentValue > aAdjustmentSeq;
    if ( ( rProp.Value >>= aAdjustmentSeq )
         && eShapeType != mso_sptActionButtonForwardNext  // we have adjustments values for these type of shape, but MSO doesn't like them
         && eShapeType != mso_sptActionButtonBackPrevious // so they are now disabled
         && OString(pShape) != "rect" //some shape types are commented out in pCustomShapeTypeTranslationTable[] & are being defaulted to rect & rect does not have adjustment values/name.
        ) {
        DBG(fprintf(stderr, "adj seq len: %d\n", int( aAdjustmentSeq.getLength() )));
        if ( bPredefinedHandlesUsed )
            EscherPropertyContainer::LookForPolarHandles( eShapeType, nAdjustmentsWhichNeedsToBeConverted );

        sal_Int32 nValue, nLength = aAdjustmentSeq.getLength();
        //aAdjustments will give info about the number of adj values for a particular geomtery.For example for hexagon aAdjustments.size() will be 2 and for circular arrow it will be 5 as per lcl_getAdjNames.
        if(aAdjustments.size() == static_cast<sal_uInt32>(nLength))// In case there is a mismatch do not write the XML_gd tag.
        {
            for( sal_Int32 i=0; i < nLength; i++ )
                if( EscherPropertyContainer::GetAdjustmentValue( aAdjustmentSeq[ i ], i, nAdjustmentsWhichNeedsToBeConverted, nValue ) )
                {
                    // If the document model doesn't have an adjustment name (e.g. shape was created from VML), then take it from the predefined list.
                    OString aAdjName;
                    if (aAdjustmentSeq[i].Name.isEmpty() && static_cast<sal_uInt32>(i) < aAdjustments.size())
                        aAdjName = aAdjustments[i];

                    mpFS->singleElementNS( XML_a, XML_gd,
                                       XML_name, aAdjustmentSeq[ i ].Name.getLength() > 0 ? USS(aAdjustmentSeq[ i ].Name) : aAdjName.getStr(),
                                       XML_fmla, OString("val " + OString::number( nValue )).getStr(),
                                       FSEND );
            }
        }
    }

    mpFS->endElementNS( XML_a, XML_avLst );
    mpFS->endElementNS(  XML_a, XML_prstGeom );
}

void DrawingML::WritePolyPolygon( const PolyPolygon& rPolyPolygon )
{
    if( rPolyPolygon.Count() < 1 )
        return;

    mpFS->startElementNS( XML_a, XML_custGeom, FSEND );
    mpFS->singleElementNS( XML_a, XML_avLst, FSEND );
    mpFS->singleElementNS( XML_a, XML_gdLst, FSEND );
    mpFS->singleElementNS( XML_a, XML_ahLst, FSEND );
    mpFS->singleElementNS( XML_a, XML_rect,
                           XML_l, "0",
                           XML_t, "0",
                           XML_r, "r",
                           XML_b, "b",
                           FSEND );

    mpFS->startElementNS( XML_a, XML_pathLst, FSEND );

    for( sal_uInt16 i = 0; i < rPolyPolygon.Count(); i ++ ) {

        const Polygon& rPoly = rPolyPolygon[ i ];
        Rectangle aRect( rPoly.GetBoundRect() );

        mpFS->startElementNS( XML_a, XML_path,
                              XML_w, I64S( aRect.GetWidth() ),
                              XML_h, I64S( aRect.GetHeight() ),
                              FSEND );

        if( rPoly.GetSize() > 0 )
        {
            mpFS->startElementNS( XML_a, XML_moveTo, FSEND );

            mpFS->singleElementNS( XML_a, XML_pt,
                                   XML_x, I64S( rPoly[ 0 ].X() - aRect.Left() ),
                                   XML_y, I64S( rPoly[ 0 ].Y() - aRect.Top() ),
                                   FSEND );

            mpFS->endElementNS( XML_a, XML_moveTo );
        }

        for( sal_uInt16 j = 1; j < rPoly.GetSize(); j ++ )
        {
            enum PolyFlags flags = rPoly.GetFlags(j);
            if( flags == POLY_CONTROL )
            {
                // a:cubicBezTo can only contain 3 a:pt elements, so we need to make sure of this
                if( j+2 < rPoly.GetSize() && rPoly.GetFlags(j+1) == POLY_CONTROL && rPoly.GetFlags(j+2) != POLY_CONTROL )
                {

                    mpFS->startElementNS( XML_a, XML_cubicBezTo, FSEND );
                    for( sal_uInt8 k = 0; k <= 2; ++k )
                    {
                        mpFS->singleElementNS( XML_a, XML_pt,
                                               XML_x, I64S( rPoly[j+k].X() - aRect.Left() ),
                                               XML_y, I64S( rPoly[j+k].Y() - aRect.Top() ),
                                               FSEND );

                    }
                    mpFS->endElementNS( XML_a, XML_cubicBezTo );
                    j += 2;
                }
            }
            else if( flags == POLY_NORMAL )
            {
                mpFS->startElementNS( XML_a, XML_lnTo, FSEND );
                mpFS->singleElementNS( XML_a, XML_pt,
                                       XML_x, I64S( rPoly[j].X() - aRect.Left() ),
                                       XML_y, I64S( rPoly[j].Y() - aRect.Top() ),
                                       FSEND );
                mpFS->endElementNS( XML_a, XML_lnTo );
            }
        }

        mpFS->endElementNS( XML_a, XML_path );
    }

    mpFS->endElementNS( XML_a, XML_pathLst );

    mpFS->endElementNS(  XML_a, XML_custGeom );
}

void DrawingML::WriteConnectorConnections( EscherConnectorListEntry& rConnectorEntry, sal_Int32 nStartID, sal_Int32 nEndID )
{
    if( nStartID != -1 )
        mpFS->singleElementNS( XML_a, XML_stCxn,
                               XML_id, I32S( nStartID ),
                               XML_idx, I64S( rConnectorEntry.GetConnectorRule( true ) ),
                               FSEND );
    if( nEndID != -1 )
        mpFS->singleElementNS( XML_a, XML_endCxn,
                               XML_id, I32S( nEndID ),
                               XML_idx, I64S( rConnectorEntry.GetConnectorRule( false ) ),
                               FSEND );
}

sal_Unicode DrawingML::SubstituteBullet( sal_Unicode cBulletId, ::com::sun::star::awt::FontDescriptor& rFontDesc )
{
    if ( IsStarSymbol(rFontDesc.Name) )
    {
        rtl_TextEncoding eCharSet = rFontDesc.CharSet;
        cBulletId = msfilter::util::bestFitOpenSymbolToMSFont(cBulletId, eCharSet, rFontDesc.Name);
        rFontDesc.CharSet = eCharSet;
    }

    return cBulletId;
}

sax_fastparser::FSHelperPtr DrawingML::CreateOutputStream (
    const OUString& sFullStream,
    const OUString& sRelativeStream,
    const Reference< XOutputStream >& xParentRelation,
    const char* sContentType,
    const char* sRelationshipType,
    OUString* pRelationshipId )
{
    OUString sRelationshipId;
    if (xParentRelation.is())
        sRelationshipId = GetFB()->addRelation( xParentRelation, OUString::createFromAscii( sRelationshipType), sRelativeStream );
    else
        sRelationshipId = GetFB()->addRelation( OUString::createFromAscii( sRelationshipType ), sRelativeStream );

    if( pRelationshipId )
        *pRelationshipId = sRelationshipId;

    sax_fastparser::FSHelperPtr p = GetFB()->openFragmentStreamWithSerializer( sFullStream, OUString::createFromAscii( sContentType ) );

    return p;
}

void DrawingML::WriteFill( Reference< XPropertySet > xPropSet )
{
    if ( !GetProperty( xPropSet, "FillStyle" ) )
        return;
    FillStyle aFillStyle( FillStyle_NONE );
    xPropSet->getPropertyValue( "FillStyle" ) >>= aFillStyle;

    if ( aFillStyle == FillStyle_SOLID && GetProperty( xPropSet, "FillTransparence" ) )
    {
        // map full transparent background to no fill
        sal_Int16 nVal = 0;
        xPropSet->getPropertyValue( "FillTransparence" ) >>= nVal;
        if ( nVal == 100 )
            aFillStyle = FillStyle_NONE;
    }

    switch( aFillStyle )
    {
    case FillStyle_SOLID :
        WriteSolidFill( xPropSet );
        break;
    case FillStyle_GRADIENT :
        WriteGradientFill( xPropSet );
        break;
    case FillStyle_BITMAP :
        WriteBlipFill( xPropSet, "FillBitmapURL" );
        break;
    case FillStyle_HATCH :
        WritePattFill( xPropSet );
        break;
    case FillStyle_NONE:
        mpFS->singleElementNS( XML_a, XML_noFill, FSEND );
        break;
    default:
        ;
    }

    return;
}

void DrawingML::WriteStyleProperties( sal_Int32 nTokenId, const Sequence< PropertyValue >& aProperties )
{
    if( aProperties.getLength() > 0 )
    {
        OUString sSchemeClr;
        sal_uInt32 nIdx = 0;
        Sequence< PropertyValue > aTransformations;
        for( sal_Int32 i=0; i < aProperties.getLength(); ++i)
            if( aProperties[i].Name == "SchemeClr" )
                aProperties[i].Value >>= sSchemeClr;
            else if( aProperties[i].Name == "Idx" )
                aProperties[i].Value >>= nIdx;
            else if( aProperties[i].Name == "Transformations" )
                aProperties[i].Value >>= aTransformations;
        mpFS->startElementNS( XML_a, nTokenId, XML_idx, I32S( nIdx ), FSEND );
        WriteColor( sSchemeClr, aTransformations );
        mpFS->endElementNS( XML_a, nTokenId );
    }
    else
        // write mock <a:*Ref> tag
        mpFS->singleElementNS( XML_a, nTokenId, XML_idx, I32S( 0 ), FSEND );
}

void DrawingML::WriteShapeStyle( Reference< XPropertySet > xPropSet )
{
    // check existence of the grab bag
    if ( !GetProperty( xPropSet, "InteropGrabBag" ) )
        return;

    // extract the relevant properties from the grab bag
    Sequence< PropertyValue > aGrabBag;
    Sequence< PropertyValue > aFillRefProperties, aLnRefProperties, aEffectRefProperties;
    mAny >>= aGrabBag;
    for( sal_Int32 i=0; i < aGrabBag.getLength(); ++i)
        if( aGrabBag[i].Name == "StyleFillRef" )
            aGrabBag[i].Value >>= aFillRefProperties;
        else if( aGrabBag[i].Name == "StyleLnRef" )
            aGrabBag[i].Value >>= aLnRefProperties;
        else if( aGrabBag[i].Name == "StyleEffectRef" )
            aGrabBag[i].Value >>= aEffectRefProperties;

    WriteStyleProperties( XML_lnRef, aLnRefProperties );
    WriteStyleProperties( XML_fillRef, aFillRefProperties );
    WriteStyleProperties( XML_effectRef, aEffectRefProperties );

    // write mock <a:fontRef>
    mpFS->singleElementNS( XML_a, XML_fontRef, XML_idx, "minor", FSEND );
}

void DrawingML::WriteShapeEffect( const OUString& sName, const Sequence< PropertyValue >& aEffectProps )
{
    if( aEffectProps.getLength() == 0 )
        return;

    // assign the proper tag and enable bContainsColor if necessary
    sal_Int32 nEffectToken = 0;
    bool bContainsColor = false;
    if( sName == "outerShdw" )
    {
        nEffectToken = FSNS( XML_a, XML_outerShdw );
        bContainsColor = true;
    }
    else if( sName == "innerShdw" )
    {
        nEffectToken = FSNS( XML_a, XML_innerShdw );
        bContainsColor = true;
    }
    else if( sName == "glow" )
    {
        nEffectToken = FSNS( XML_a, XML_glow );
        bContainsColor = true;
    }
    else if( sName == "softEdge" )
        nEffectToken = FSNS( XML_a, XML_softEdge );
    else if( sName == "reflection" )
        nEffectToken = FSNS( XML_a, XML_reflection );
    else if( sName == "blur" )
        nEffectToken = FSNS( XML_a, XML_blur );

    OUString sSchemeClr;
    sal_uInt32 nRgbClr = 0;
    sal_Int32 nAlpha = MAX_PERCENT;
    Sequence< PropertyValue > aTransformations;
    sax_fastparser::FastAttributeList *aOuterShdwAttrList = mpFS->createAttrList();
    sax_fastparser::XFastAttributeListRef xOuterShdwAttrList( aOuterShdwAttrList );
    for( sal_Int32 i=0; i < aEffectProps.getLength(); ++i )
    {
        if( aEffectProps[i].Name == "Attribs" )
        {
            // read tag attributes
            uno::Sequence< beans::PropertyValue > aOuterShdwProps;
            aEffectProps[i].Value >>= aOuterShdwProps;
            for( sal_Int32 j=0; j < aOuterShdwProps.getLength(); ++j )
            {
                if( aOuterShdwProps[j].Name == "algn" )
                {
                    OUString sVal;
                    aOuterShdwProps[j].Value >>= sVal;
                    aOuterShdwAttrList->add( XML_algn, OUStringToOString( sVal, RTL_TEXTENCODING_UTF8 ).getStr() );
                }
                else if( aOuterShdwProps[j].Name == "blurRad" )
                {
                    sal_Int32 nVal = 0;
                    aOuterShdwProps[j].Value >>= nVal;
                    aOuterShdwAttrList->add( XML_blurRad, OString::number( nVal ).getStr() );
                }
                else if( aOuterShdwProps[j].Name == "dir" )
                {
                    sal_Int32 nVal = 0;
                    aOuterShdwProps[j].Value >>= nVal;
                    aOuterShdwAttrList->add( XML_dir, OString::number( nVal ).getStr() );
                }
                else if( aOuterShdwProps[j].Name == "dist" )
                {
                    sal_Int32 nVal = 0;
                    aOuterShdwProps[j].Value >>= nVal;
                    aOuterShdwAttrList->add( XML_dist, OString::number( nVal ).getStr() );
                }
                else if( aOuterShdwProps[j].Name == "kx" )
                {
                    sal_Int32 nVal = 0;
                    aOuterShdwProps[j].Value >>= nVal;
                    aOuterShdwAttrList->add( XML_kx, OString::number( nVal ).getStr() );
                }
                else if( aOuterShdwProps[j].Name == "ky" )
                {
                    sal_Int32 nVal = 0;
                    aOuterShdwProps[j].Value >>= nVal;
                    aOuterShdwAttrList->add( XML_ky, OString::number( nVal ).getStr() );
                }
                else if( aOuterShdwProps[j].Name == "rotWithShape" )
                {
                    sal_Int32 nVal = 0;
                    aOuterShdwProps[j].Value >>= nVal;
                    aOuterShdwAttrList->add( XML_rotWithShape, OString::number( nVal ).getStr() );
                }
                else if( aOuterShdwProps[j].Name == "sx" )
                {
                    sal_Int32 nVal = 0;
                    aOuterShdwProps[j].Value >>= nVal;
                    aOuterShdwAttrList->add( XML_sx, OString::number( nVal ).getStr() );
                }
                else if( aOuterShdwProps[j].Name == "sy" )
                {
                    sal_Int32 nVal = 0;
                    aOuterShdwProps[j].Value >>= nVal;
                    aOuterShdwAttrList->add( XML_sy, OString::number( nVal ).getStr() );
                }
                else if( aOuterShdwProps[j].Name == "rad" )
                {
                    sal_Int32 nVal = 0;
                    aOuterShdwProps[j].Value >>= nVal;
                    aOuterShdwAttrList->add( XML_rad, OString::number( nVal ).getStr() );
                }
                else if( aOuterShdwProps[j].Name == "endA" )
                {
                    sal_Int32 nVal = 0;
                    aOuterShdwProps[j].Value >>= nVal;
                    aOuterShdwAttrList->add( XML_endA, OString::number( nVal ).getStr() );
                }
                else if( aOuterShdwProps[j].Name == "endPos" )
                {
                    sal_Int32 nVal = 0;
                    aOuterShdwProps[j].Value >>= nVal;
                    aOuterShdwAttrList->add( XML_endPos, OString::number( nVal ).getStr() );
                }
                else if( aOuterShdwProps[j].Name == "fadeDir" )
                {
                    sal_Int32 nVal = 0;
                    aOuterShdwProps[j].Value >>= nVal;
                    aOuterShdwAttrList->add( XML_fadeDir, OString::number( nVal ).getStr() );
                }
                else if( aOuterShdwProps[j].Name == "stA" )
                {
                    sal_Int32 nVal = 0;
                    aOuterShdwProps[j].Value >>= nVal;
                    aOuterShdwAttrList->add( XML_stA, OString::number( nVal ).getStr() );
                }
                else if( aOuterShdwProps[j].Name == "stPos" )
                {
                    sal_Int32 nVal = 0;
                    aOuterShdwProps[j].Value >>= nVal;
                    aOuterShdwAttrList->add( XML_stPos, OString::number( nVal ).getStr() );
                }
                else if( aOuterShdwProps[j].Name == "grow" )
                {
                    sal_Int32 nVal = 0;
                    aOuterShdwProps[j].Value >>= nVal;
                    aOuterShdwAttrList->add( XML_grow, OString::number( nVal ).getStr() );
                }
            }
        }
        else if(aEffectProps[i].Name == "RgbClr")
        {
            aEffectProps[i].Value >>= nRgbClr;
        }
        else if(aEffectProps[i].Name == "RgbClrTransparency")
        {
            sal_Int32 nTransparency;
            if (aEffectProps[i].Value >>= nTransparency)
                // Calculate alpha value (see oox/source/drawingml/color.cxx : getTransparency())
                nAlpha = MAX_PERCENT - ( PER_PERCENT * nTransparency );
        }
        else if(aEffectProps[i].Name == "SchemeClr")
        {
            aEffectProps[i].Value >>= sSchemeClr;
        }
        else if(aEffectProps[i].Name == "SchemeClrTransformations")
        {
            aEffectProps[i].Value >>= aTransformations;
        }
    }

    if( nEffectToken > 0 )
    {
        mpFS->startElement( nEffectToken, xOuterShdwAttrList );

        if( bContainsColor )
        {
            if( sSchemeClr.isEmpty() )
                WriteColor( nRgbClr, nAlpha );
            else
                WriteColor( sSchemeClr, aTransformations );
        }

        mpFS->endElement( nEffectToken );
    }
}

void DrawingML::WriteShapeEffects( Reference< XPropertySet > rXPropSet )
{
    if( !GetProperty( rXPropSet, "InteropGrabBag" ) )
        return;

    Sequence< PropertyValue > aGrabBag, aEffects;
    mAny >>= aGrabBag;
    for( sal_Int32 i=0; i < aGrabBag.getLength(); ++i )
    {
        if( aGrabBag[i].Name == "EffectProperties" )
        {
            aGrabBag[i].Value >>= aEffects;
            break;
        }
    }
    if( aEffects.getLength() == 0 )
        return;

    mpFS->startElementNS(XML_a, XML_effectLst, FSEND);

    for( sal_Int32 i=0; i < aEffects.getLength(); ++i )
    {
        Sequence< PropertyValue > aEffectProps;
        aEffects[i].Value >>= aEffectProps;
        WriteShapeEffect( aEffects[i].Name, aEffectProps );
    }

    mpFS->endElementNS(XML_a, XML_effectLst);
}

void DrawingML::WriteShape3DEffects( Reference< XPropertySet > xPropSet )
{
    // check existence of the grab bag
    if( !GetProperty( xPropSet, "InteropGrabBag" ) )
        return;

    // extract the relevant properties from the grab bag
    Sequence< PropertyValue > aGrabBag, aEffectProps, aLightRigProps, aShape3DProps;
    mAny >>= aGrabBag;
    for( sal_Int32 i=0; i < aGrabBag.getLength(); ++i )
        if( aGrabBag[i].Name == "3DEffectProperties" )
        {
            Sequence< PropertyValue > a3DEffectProps;
            aGrabBag[i].Value >>= a3DEffectProps;
            for( sal_Int32 j=0; j < a3DEffectProps.getLength(); ++j )
            {
                if( a3DEffectProps[j].Name == "Camera" )
                    a3DEffectProps[j].Value >>= aEffectProps;
                else if( a3DEffectProps[j].Name == "LightRig" )
                    a3DEffectProps[j].Value >>= aLightRigProps;
                else if( a3DEffectProps[j].Name == "Shape3D" )
                    a3DEffectProps[j].Value >>= aShape3DProps;
            }
            break;
        }
    if( aEffectProps.getLength() == 0 && aLightRigProps.getLength() == 0 && aShape3DProps.getLength() == 0 )
        return;

    bool bCameraRotationPresent = false;
    sax_fastparser::FastAttributeList *aCameraAttrList = mpFS->createAttrList();
    sax_fastparser::XFastAttributeListRef xCameraAttrList( aCameraAttrList );
    sax_fastparser::FastAttributeList *aCameraRotationAttrList = mpFS->createAttrList();
    sax_fastparser::XFastAttributeListRef xRotAttrList( aCameraRotationAttrList );
    for( sal_Int32 i=0; i < aEffectProps.getLength(); ++i )
    {
        if( aEffectProps[i].Name == "prst" )
        {
            OUString sVal;
            aEffectProps[i].Value >>= sVal;
            aCameraAttrList->add( XML_prst, OUStringToOString( sVal, RTL_TEXTENCODING_UTF8 ).getStr() );
        }
        else if( aEffectProps[i].Name == "fov" )
        {
            float fVal = 0;
            aEffectProps[i].Value >>= fVal;
            aCameraAttrList->add( XML_fov, OString::number( fVal * 60000 ).getStr() );
        }
        else if( aEffectProps[i].Name == "zoom" )
        {
            float fVal = 1;
            aEffectProps[i].Value >>= fVal;
            aCameraAttrList->add( XML_zoom, OString::number( fVal * 100000 ).getStr() );
        }
        else if( aEffectProps[i].Name == "rotLat" ||
                aEffectProps[i].Name == "rotLon" ||
                aEffectProps[i].Name == "rotRev" )
        {
            sal_Int32 nVal = 0, nToken = XML_none;
            aEffectProps[i].Value >>= nVal;
            if( aEffectProps[i].Name == "rotLat" )
                nToken = XML_lat;
            else if( aEffectProps[i].Name == "rotLon" )
                nToken = XML_lon;
            else if( aEffectProps[i].Name == "rotRev" )
                nToken = XML_rev;
            aCameraRotationAttrList->add( nToken, OString::number( nVal ).getStr() );
            bCameraRotationPresent = true;
        }
    }

    bool bLightRigRotationPresent = false;
    sax_fastparser::FastAttributeList *aLightRigAttrList = mpFS->createAttrList();
    sax_fastparser::XFastAttributeListRef xLightAttrList( aLightRigAttrList );
    sax_fastparser::FastAttributeList *aLightRigRotationAttrList = mpFS->createAttrList();
    sax_fastparser::XFastAttributeListRef xLightRotAttrList( aLightRigRotationAttrList );
    for( sal_Int32 i=0; i < aLightRigProps.getLength(); ++i )
    {
        if( aLightRigProps[i].Name == "rig" || aLightRigProps[i].Name == "dir" )
        {
            OUString sVal;
            sal_Int32 nToken = XML_none;
            aLightRigProps[i].Value >>= sVal;
            if( aLightRigProps[i].Name == "rig" )
                nToken = XML_rig;
            else if( aLightRigProps[i].Name == "dir" )
                nToken = XML_dir;
            aLightRigAttrList->add( nToken, OUStringToOString( sVal, RTL_TEXTENCODING_UTF8 ).getStr() );
        }
        else if( aLightRigProps[i].Name == "rotLat" ||
                aLightRigProps[i].Name == "rotLon" ||
                aLightRigProps[i].Name == "rotRev" )
        {
            sal_Int32 nVal = 0, nToken = XML_none;
            aLightRigProps[i].Value >>= nVal;
            if( aLightRigProps[i].Name == "rotLat" )
                nToken = XML_lat;
            else if( aLightRigProps[i].Name == "rotLon" )
                nToken = XML_lon;
            else if( aLightRigProps[i].Name == "rotRev" )
                nToken = XML_rev;
            aLightRigRotationAttrList->add( nToken, OString::number( nVal ).getStr() );
            bLightRigRotationPresent = true;
        }
    }

    mpFS->startElementNS( XML_a, XML_scene3d, FSEND );

    if( aEffectProps.getLength() > 0 )
    {
        mpFS->startElementNS( XML_a, XML_camera, xCameraAttrList );
        if( bCameraRotationPresent )
        {
            mpFS->singleElementNS( XML_a, XML_rot, xRotAttrList );
        }
        mpFS->endElementNS( XML_a, XML_camera );
    }
    else
        // a:camera with Word default values - Word won't open the document if this is not present
        mpFS->singleElementNS( XML_a, XML_camera, XML_prst, "orthographicFront", FSEND );

    if( aEffectProps.getLength() > 0 )
    {
        mpFS->startElementNS( XML_a, XML_lightRig, xLightAttrList );
        if( bLightRigRotationPresent )
        {
            mpFS->singleElementNS( XML_a, XML_rot, xLightRotAttrList );
        }
        mpFS->endElementNS( XML_a, XML_lightRig );
    }
    else
        // a:lightRig with Word default values - Word won't open the document if this is not present
        mpFS->singleElementNS( XML_a, XML_lightRig, XML_rig, "threePt", XML_dir, "t", FSEND );

    mpFS->endElementNS( XML_a, XML_scene3d );

    if( aShape3DProps.getLength() == 0 )
        return;

    bool bBevelTPresent = false, bBevelBPresent = false;
    Sequence< PropertyValue > aExtrusionColorProps, aContourColorProps;
    sax_fastparser::FastAttributeList *aBevelTAttrList = mpFS->createAttrList();
    sax_fastparser::XFastAttributeListRef xBevelTAttrList( aBevelTAttrList );
    sax_fastparser::FastAttributeList *aBevelBAttrList = mpFS->createAttrList();
    sax_fastparser::XFastAttributeListRef xBevelBAttrList( aBevelBAttrList );
    sax_fastparser::FastAttributeList *aShape3DAttrList = mpFS->createAttrList();
    for( sal_Int32 i=0; i < aShape3DProps.getLength(); ++i )
    {
        if( aShape3DProps[i].Name == "extrusionH" || aShape3DProps[i].Name == "contourW" || aShape3DProps[i].Name == "z" )
        {
            sal_Int32 nVal = 0, nToken = XML_none;
            aShape3DProps[i].Value >>= nVal;
            if( aShape3DProps[i].Name == "extrusionH" )
                nToken = XML_extrusionH;
            else if( aShape3DProps[i].Name == "contourW" )
                nToken = XML_contourW;
            else if( aShape3DProps[i].Name == "z" )
                nToken = XML_z;
            aShape3DAttrList->add( nToken, OString::number( nVal ).getStr() );
        }
        else if( aShape3DProps[i].Name == "prstMaterial" )
        {
            OUString sVal;
            aShape3DProps[i].Value >>= sVal;
            aShape3DAttrList->add( XML_prstMaterial, OUStringToOString( sVal, RTL_TEXTENCODING_UTF8 ).getStr() );
        }
        else if( aShape3DProps[i].Name == "extrusionClr" )
        {
            aShape3DProps[i].Value >>= aExtrusionColorProps;
        }
        else if( aShape3DProps[i].Name == "contourClr" )
        {
            aShape3DProps[i].Value >>= aContourColorProps;
        }
        else if( aShape3DProps[i].Name == "bevelT" || aShape3DProps[i].Name == "bevelB" )
        {
            Sequence< PropertyValue > aBevelProps;
            aShape3DProps[i].Value >>= aBevelProps;
            if ( aBevelProps.getLength() == 0 )
                continue;

            sax_fastparser::FastAttributeList *aBevelAttrList = NULL;
            if( aShape3DProps[i].Name == "bevelT" )
            {
                bBevelTPresent = true;
                aBevelAttrList = aBevelTAttrList;
            }
            else
            {
                bBevelBPresent = true;
                aBevelAttrList = aBevelBAttrList;
            }
            for( sal_Int32 j=0; j < aBevelProps.getLength(); ++j )
            {
                if( aBevelProps[j].Name == "w" || aBevelProps[j].Name == "h" )
                {
                    sal_Int32 nVal = 0, nToken = XML_none;
                    aBevelProps[j].Value >>= nVal;
                    if( aBevelProps[j].Name == "w" )
                        nToken = XML_w;
                    else if( aBevelProps[j].Name == "h" )
                        nToken = XML_h;
                    aBevelAttrList->add( nToken, OString::number( nVal ).getStr() );
                }
                else  if( aBevelProps[j].Name == "prst" )
                {
                    OUString sVal;
                    aBevelProps[j].Value >>= sVal;
                    aBevelAttrList->add( XML_prst, OUStringToOString( sVal, RTL_TEXTENCODING_UTF8 ).getStr() );
                }
            }

        }
    }

    sax_fastparser::XFastAttributeListRef xAttrList( aShape3DAttrList );
    mpFS->startElementNS( XML_a, XML_sp3d, xAttrList );
    if( bBevelTPresent )
    {
        mpFS->singleElementNS( XML_a, XML_bevelT, xBevelTAttrList );
    }
    if( bBevelBPresent )
    {
        mpFS->singleElementNS( XML_a, XML_bevelB, xBevelBAttrList );
    }
    if( aExtrusionColorProps.getLength() > 0 )
    {
        OUString sSchemeClr;
        sal_Int32 nColor(0);
        sal_Int32 nTransparency(0);
        Sequence< PropertyValue > aColorTransformations;
        for( sal_Int32 i=0; i < aExtrusionColorProps.getLength(); ++i )
        {
            if( aExtrusionColorProps[i].Name == "schemeClr" )
                aExtrusionColorProps[i].Value >>= sSchemeClr;
            else if( aExtrusionColorProps[i].Name == "schemeClrTransformations" )
                aExtrusionColorProps[i].Value >>= aColorTransformations;
            else if( aExtrusionColorProps[i].Name == "rgbClr" )
                aExtrusionColorProps[i].Value >>= nColor;
            else if( aExtrusionColorProps[i].Name == "rgbClrTransparency" )
                aExtrusionColorProps[i].Value >>= nTransparency;
        }
        mpFS->startElementNS( XML_a, XML_extrusionClr, FSEND );

        if( sSchemeClr.isEmpty() )
            WriteColor( nColor, MAX_PERCENT - ( PER_PERCENT * nTransparency ) );
        else
            WriteColor( sSchemeClr, aColorTransformations );

        mpFS->endElementNS( XML_a, XML_extrusionClr );
    }
    if( aContourColorProps.getLength() > 0 )
    {
        OUString sSchemeClr;
        sal_Int32 nColor(0);
        sal_Int32 nTransparency(0);
        Sequence< PropertyValue > aColorTransformations;
        for( sal_Int32 i=0; i < aContourColorProps.getLength(); ++i )
        {
            if( aContourColorProps[i].Name == "schemeClr" )
                aContourColorProps[i].Value >>= sSchemeClr;
            else if( aContourColorProps[i].Name == "schemeClrTransformations" )
                aContourColorProps[i].Value >>= aColorTransformations;
            else if( aContourColorProps[i].Name == "rgbClr" )
                aContourColorProps[i].Value >>= nColor;
            else if( aContourColorProps[i].Name == "rgbClrTransparency" )
                aContourColorProps[i].Value >>= nTransparency;
        }
        mpFS->startElementNS( XML_a, XML_contourClr, FSEND );

        if( sSchemeClr.isEmpty() )
            WriteColor( nColor, MAX_PERCENT - ( PER_PERCENT * nTransparency ) );
        else
            WriteColor( sSchemeClr, aContourColorProps );

        mpFS->endElementNS( XML_a, XML_contourClr );
    }
    mpFS->endElementNS( XML_a, XML_sp3d );
}

void DrawingML::WriteArtisticEffect( Reference< XPropertySet > rXPropSet )
{
    if( !GetProperty( rXPropSet, "InteropGrabBag" ) )
        return;

    PropertyValue aEffect;
    Sequence< PropertyValue > aGrabBag;
    mAny >>= aGrabBag;
    for( sal_Int32 i=0; i < aGrabBag.getLength(); ++i )
    {
        if( aGrabBag[i].Name == "ArtisticEffectProperties" )
        {
            aGrabBag[i].Value >>= aEffect;
            break;
        }
    }
    sal_Int32 nEffectToken = ArtisticEffectProperties::getEffectToken( aEffect.Name );
    if( nEffectToken == XML_none )
        return;

    Sequence< PropertyValue > aAttrs;
    aEffect.Value >>= aAttrs;
    sax_fastparser::FastAttributeList *aAttrList = mpFS->createAttrList();
    OString sRelId;
    for( sal_Int32 i=0; i < aAttrs.getLength(); ++i )
    {
        sal_Int32 nToken = ArtisticEffectProperties::getEffectToken( aAttrs[i].Name );
        if( nToken != XML_none )
        {
            sal_Int32 nVal = 0;
            aAttrs[i].Value >>= nVal;
            aAttrList->add( nToken, OString::number( nVal ).getStr() );
        }
        else if( aAttrs[i].Name == "OriginalGraphic" )
        {
            Sequence< PropertyValue > aGraphic;
            aAttrs[i].Value >>= aGraphic;
            Sequence< sal_Int8 > aGraphicData;
            OUString sGraphicId;
            for( sal_Int32 j=0; j < aGraphic.getLength(); ++j )
            {
                if( aGraphic[j].Name == "Id" )
                    aGraphic[j].Value >>= sGraphicId;
                else if( aGraphic[j].Name == "Data" )
                    aGraphic[j].Value >>= aGraphicData;
            }
            sRelId = WriteWdpPicture( sGraphicId, aGraphicData );
        }
    }

    mpFS->startElementNS( XML_a, XML_extLst, FSEND );
    mpFS->startElementNS( XML_a, XML_ext,
                          XML_uri, "{BEBA8EAE-BF5A-486C-A8C5-ECC9F3942E4B}",
                          FSEND );
    mpFS->startElementNS( XML_a14, XML_imgProps,
                          FSNS( XML_xmlns, XML_a14 ), "http://schemas.microsoft.com/office/drawing/2010/main",
                          FSEND );
    mpFS->startElementNS( XML_a14, XML_imgLayer,
                          FSNS( XML_r, XML_embed), sRelId.getStr(),
                          FSEND );
    mpFS->startElementNS( XML_a14, XML_imgEffect, FSEND );

    sax_fastparser::XFastAttributeListRef xAttrList( aAttrList );
    mpFS->singleElementNS( XML_a14, nEffectToken, xAttrList );

    mpFS->endElementNS( XML_a14, XML_imgEffect );
    mpFS->endElementNS( XML_a14, XML_imgLayer );
    mpFS->endElementNS( XML_a14, XML_imgProps );
    mpFS->endElementNS( XML_a, XML_ext );
    mpFS->endElementNS( XML_a, XML_extLst );
}

OString DrawingML::WriteWdpPicture( const OUString& rFileId, const Sequence< sal_Int8 >& rPictureData )
{
    std::map<OUString, OUString>::iterator aCachedItem = maWdpCache.find( rFileId );
    if( aCachedItem != maWdpCache.end() )
        return OUStringToOString( aCachedItem->second, RTL_TEXTENCODING_UTF8 );

    OUString sFileName = "media/hdphoto" + OUString::number( mnWdpImageCounter++ ) + ".wdp";
    uno::Reference< io::XOutputStream > xOutStream =
            mpFB->openFragmentStream( "word/" + sFileName,
                                      "image/vnd.ms-photo" );
    OUString sId;
    xOutStream->writeBytes( rPictureData );
    xOutStream->closeOutput();

    sId = mpFB->addRelation( mpFS->getOutputStream(),
                             "http://schemas.microsoft.com/office/2007/relationships/hdphoto",
                             sFileName, false );

    maWdpCache[rFileId] = sId;
    return OUStringToOString( sId, RTL_TEXTENCODING_UTF8 );
}

}
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
