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

#include "DataPointItemConverter.hxx"
#include "SchWhichPairs.hxx"
#include "macros.hxx"
#include "ItemPropertyMap.hxx"

#include "GraphicPropertyItemConverter.hxx"
#include "CharacterPropertyItemConverter.hxx"
#include "StatisticsItemConverter.hxx"
#include "SeriesOptionsItemConverter.hxx"
#include "DataSeriesHelper.hxx"
#include "DiagramHelper.hxx"
#include "ChartModelHelper.hxx"
#include "ChartTypeHelper.hxx"
#include <unonames.hxx>

#include <svx/chrtitem.hxx>
#include <com/sun/star/chart2/DataPointLabel.hpp>
#include <com/sun/star/chart2/Symbol.hpp>

#include <svx/xflclit.hxx>
#include <svl/intitem.hxx>
#include <editeng/sizeitem.hxx>
#include <svl/stritem.hxx>
#include <editeng/brushitem.hxx>
#include <svl/ilstitem.hxx>
#include <vcl/graph.hxx>
#include <com/sun/star/graphic/XGraphic.hpp>

#include <svx/tabline.hxx>

#include <functional>
#include <algorithm>

using namespace ::com::sun::star;
using namespace ::com::sun::star::chart2;
using ::com::sun::star::uno::Reference;

namespace chart { namespace wrapper {

namespace {

ItemPropertyMapType & lcl_GetDataPointPropertyMap()
{
    static ItemPropertyMapType aDataPointPropertyMap(
        MakeItemPropertyMap
        IPM_MAP_ENTRY( SCHATTR_STYLE_SHAPE, "Geometry3D", 0 )
        );

    return aDataPointPropertyMap;
};

sal_Int32 lcl_getSymbolStyleForSymbol( const chart2::Symbol & rSymbol )
{
    sal_Int32 nStyle = SVX_SYMBOLTYPE_UNKNOWN;
    switch( rSymbol.Style )
    {
        case chart2::SymbolStyle_NONE:
            nStyle = SVX_SYMBOLTYPE_NONE;
            break;
        case chart2::SymbolStyle_AUTO:
            nStyle = SVX_SYMBOLTYPE_AUTO;
            break;
        case chart2::SymbolStyle_GRAPHIC:
            nStyle = SVX_SYMBOLTYPE_BRUSHITEM;
            break;
        case chart2::SymbolStyle_STANDARD:
            nStyle = rSymbol.StandardSymbol;
            break;

        case chart2::SymbolStyle_POLYGON:
            // to avoid warning
        case chart2::SymbolStyle_MAKE_FIXED_SIZE:
            // nothing
            break;
    }
    return nStyle;
}

bool lcl_NumberFormatFromItemToPropertySet( sal_uInt16 nWhichId, const SfxItemSet & rItemSet, const uno::Reference< beans::XPropertySet > & xPropertySet, bool bOverwriteAttributedDataPointsAlso  )
{
    bool bChanged = false;
    if( !xPropertySet.is() )
        return bChanged;
    OUString aPropertyName = (SID_ATTR_NUMBERFORMAT_VALUE==nWhichId) ? OUString(CHART_UNONAME_NUMFMT) : OUString( "PercentageNumberFormat" );
    sal_uInt16 nSourceWhich = (SID_ATTR_NUMBERFORMAT_VALUE==nWhichId) ? SID_ATTR_NUMBERFORMAT_SOURCE : SCHATTR_PERCENT_NUMBERFORMAT_SOURCE;

    if( SFX_ITEM_SET != rItemSet.GetItemState( nSourceWhich ) )
        return bChanged;

    uno::Any aValue;
    bool bUseSourceFormat = (static_cast< const SfxBoolItem & >(
                rItemSet.Get( nSourceWhich )).GetValue() );
    if( !bUseSourceFormat )
    {
        SfxItemState aState = rItemSet.GetItemState( nWhichId );
        if( aState == SFX_ITEM_SET )
        {
            sal_Int32 nFmt = static_cast< sal_Int32 >(
                static_cast< const SfxUInt32Item & >(
                    rItemSet.Get( nWhichId )).GetValue());
            aValue = uno::makeAny(nFmt);
        }
        else
            return bChanged;
    }

    uno::Any aOldValue( xPropertySet->getPropertyValue(aPropertyName) );
    if( bOverwriteAttributedDataPointsAlso )
    {
        Reference< chart2::XDataSeries > xSeries( xPropertySet, uno::UNO_QUERY);
        if( aValue != aOldValue ||
            ::chart::DataSeriesHelper::hasAttributedDataPointDifferentValue( xSeries, aPropertyName, aOldValue ) )
        {
            ::chart::DataSeriesHelper::setPropertyAlsoToAllAttributedDataPoints( xSeries, aPropertyName, aValue );
            bChanged = true;
        }
    }
    else if( aOldValue != aValue )
    {
        xPropertySet->setPropertyValue(aPropertyName, aValue );
        bChanged = true;
    }
    return bChanged;
}

bool lcl_UseSourceFormatFromItemToPropertySet( sal_uInt16 nWhichId, const SfxItemSet & rItemSet, const uno::Reference< beans::XPropertySet > & xPropertySet, bool bOverwriteAttributedDataPointsAlso  )
{
    bool bChanged = false;
    if( !xPropertySet.is() )
        return bChanged;
    OUString aPropertyName = (SID_ATTR_NUMBERFORMAT_SOURCE==nWhichId) ? OUString(CHART_UNONAME_NUMFMT) : OUString( "PercentageNumberFormat" );
    sal_uInt16 nFormatWhich = (SID_ATTR_NUMBERFORMAT_SOURCE==nWhichId) ? SID_ATTR_NUMBERFORMAT_VALUE : SCHATTR_PERCENT_NUMBERFORMAT_VALUE;

    if( SFX_ITEM_SET != rItemSet.GetItemState( nWhichId ) )
        return bChanged;

    uno::Any aNewValue;
    bool bUseSourceFormat = (static_cast< const SfxBoolItem & >(
                rItemSet.Get( nWhichId )).GetValue() );
    if( !bUseSourceFormat )
    {
        SfxItemState aState = rItemSet.GetItemState( nFormatWhich );
        if( aState == SFX_ITEM_SET )
        {
            sal_Int32 nFormatKey = static_cast< sal_Int32 >(
            static_cast< const SfxUInt32Item & >(
                rItemSet.Get( nFormatWhich )).GetValue());
            aNewValue <<= nFormatKey;
        }
        else
            return bChanged;
    }

    uno::Any aOldValue( xPropertySet->getPropertyValue(aPropertyName) );
    if( bOverwriteAttributedDataPointsAlso )
    {
        Reference< chart2::XDataSeries > xSeries( xPropertySet, uno::UNO_QUERY);
        if( aNewValue != aOldValue ||
            ::chart::DataSeriesHelper::hasAttributedDataPointDifferentValue( xSeries, aPropertyName, aOldValue ) )
        {
            ::chart::DataSeriesHelper::setPropertyAlsoToAllAttributedDataPoints( xSeries, aPropertyName, aNewValue );
            bChanged = true;
        }
    }
    else if( aOldValue != aNewValue )
    {
        xPropertySet->setPropertyValue( aPropertyName, aNewValue );
        bChanged = true;
    }

    return bChanged;
}

} // anonymous namespace

DataPointItemConverter::DataPointItemConverter(
    const uno::Reference< frame::XModel > & xChartModel,
    const uno::Reference< uno::XComponentContext > & xContext,
    const uno::Reference< beans::XPropertySet > & rPropertySet,
    const uno::Reference< XDataSeries > & xSeries,
    SfxItemPool& rItemPool,
    SdrModel& rDrawModel,
    const uno::Reference<lang::XMultiServiceFactory>& xNamedPropertyContainerFactory,
    GraphicPropertyItemConverter::eGraphicObjectType eMapTo,
    const awt::Size* pRefSize,
    bool bDataSeries,
    bool bUseSpecialFillColor,
    sal_Int32 nSpecialFillColor,
    bool bOverwriteLabelsForAttributedDataPointsAlso,
    sal_Int32 nNumberFormat,
    sal_Int32 nPercentNumberFormat ) :
        ItemConverter( rPropertySet, rItemPool ),
        m_bDataSeries( bDataSeries ),
        m_bOverwriteLabelsForAttributedDataPointsAlso(m_bDataSeries && bOverwriteLabelsForAttributedDataPointsAlso),
        m_bUseSpecialFillColor(bUseSpecialFillColor),
        m_nSpecialFillColor(nSpecialFillColor),
        m_nNumberFormat(nNumberFormat),
        m_nPercentNumberFormat(nPercentNumberFormat),
        m_aAvailableLabelPlacements(),
        m_bForbidPercentValue(true)
{
    m_aConverters.push_back( new GraphicPropertyItemConverter(
                                 rPropertySet, rItemPool, rDrawModel, xNamedPropertyContainerFactory, eMapTo ));
    m_aConverters.push_back( new CharacterPropertyItemConverter(rPropertySet, rItemPool, pRefSize, "ReferencePageSize"));
    if( bDataSeries )
    {
        m_aConverters.push_back( new StatisticsItemConverter( xChartModel, rPropertySet, rItemPool ));
        m_aConverters.push_back( new SeriesOptionsItemConverter( xChartModel, xContext, rPropertySet, rItemPool ));
    }

    uno::Reference< XDiagram > xDiagram( ChartModelHelper::findDiagram(xChartModel) );
    uno::Reference< XChartType > xChartType( DiagramHelper::getChartTypeOfSeries( xDiagram , xSeries ) );
    bool bFound = false;
    bool bAmbiguous = false;
    bool bSwapXAndY = DiagramHelper::getVertical( xDiagram, bFound, bAmbiguous );
    m_aAvailableLabelPlacements = ChartTypeHelper::getSupportedLabelPlacements( xChartType, DiagramHelper::getDimension( xDiagram ), bSwapXAndY, xSeries );

    m_bForbidPercentValue = AxisType::CATEGORY != ChartTypeHelper::getAxisType( xChartType, 0 );
}

DataPointItemConverter::~DataPointItemConverter()
{
    ::std::for_each(m_aConverters.begin(), m_aConverters.end(), boost::checked_deleter<ItemConverter>());
}

void DataPointItemConverter::FillItemSet( SfxItemSet & rOutItemSet ) const
{
    ::std::for_each( m_aConverters.begin(), m_aConverters.end(),
                     FillItemSetFunc( rOutItemSet ));

    // own items
    ItemConverter::FillItemSet( rOutItemSet );

    if( m_bUseSpecialFillColor )
    {
        Color aColor(m_nSpecialFillColor);
        rOutItemSet.Put( XFillColorItem( OUString(), aColor ) );
    }
}

bool DataPointItemConverter::ApplyItemSet( const SfxItemSet & rItemSet )
{
    bool bResult = false;

    ::std::for_each( m_aConverters.begin(), m_aConverters.end(),
                     ApplyItemSetFunc( rItemSet, bResult ));

    // own items
    return ItemConverter::ApplyItemSet( rItemSet ) || bResult;
}

const sal_uInt16 * DataPointItemConverter::GetWhichPairs() const
{
    // must span all used items!
    if( m_bDataSeries )
        return nRowWhichPairs;
    return nDataPointWhichPairs;
}

bool DataPointItemConverter::GetItemProperty( tWhichIdType nWhichId, tPropertyNameWithMemberId & rOutProperty ) const
{
    ItemPropertyMapType & rMap( lcl_GetDataPointPropertyMap());
    ItemPropertyMapType::const_iterator aIt( rMap.find( nWhichId ));

    if( aIt == rMap.end())
        return false;

    rOutProperty =(*aIt).second;
    return true;
}

bool DataPointItemConverter::ApplySpecialItem(
    sal_uInt16 nWhichId, const SfxItemSet & rItemSet )
    throw( uno::Exception )
{
    bool bChanged = false;

    switch( nWhichId )
    {
        case SCHATTR_DATADESCR_SHOW_NUMBER:
        case SCHATTR_DATADESCR_SHOW_PERCENTAGE:
        case SCHATTR_DATADESCR_SHOW_CATEGORY:
        case SCHATTR_DATADESCR_SHOW_SYMBOL:
        {
            const SfxBoolItem & rItem = static_cast< const SfxBoolItem & >( rItemSet.Get( nWhichId ));

            uno::Any aOldValue = GetPropertySet()->getPropertyValue(CHART_UNONAME_LABEL);
            chart2::DataPointLabel aLabel;
            if( aOldValue >>= aLabel )
            {
                sal_Bool& rValue = (SCHATTR_DATADESCR_SHOW_NUMBER==nWhichId) ? aLabel.ShowNumber : (
                    (SCHATTR_DATADESCR_SHOW_PERCENTAGE==nWhichId) ? aLabel.ShowNumberInPercent : (
                    (SCHATTR_DATADESCR_SHOW_CATEGORY==nWhichId) ? aLabel.ShowCategoryName : aLabel.ShowLegendSymbol ));
                bool bOldValue = rValue;
                rValue = rItem.GetValue();
                if( m_bOverwriteLabelsForAttributedDataPointsAlso )
                {
                    Reference< chart2::XDataSeries > xSeries( GetPropertySet(), uno::UNO_QUERY);
                    if( (bOldValue ? 1 : 0) != rValue ||
                        DataSeriesHelper::hasAttributedDataPointDifferentValue( xSeries, CHART_UNONAME_LABEL , aOldValue ) )
                    {
                        DataSeriesHelper::setPropertyAlsoToAllAttributedDataPoints( xSeries, CHART_UNONAME_LABEL , uno::makeAny( aLabel ) );
                        bChanged = true;
                    }
                }
                else if( (bOldValue ? 1 : 0) != rValue )
                {
                    GetPropertySet()->setPropertyValue(CHART_UNONAME_LABEL , uno::makeAny(aLabel));
                    bChanged = true;
                }
            }
        }
        break;

        case SID_ATTR_NUMBERFORMAT_VALUE:
        case SCHATTR_PERCENT_NUMBERFORMAT_VALUE:  //fall through intended
        {
            bChanged = lcl_NumberFormatFromItemToPropertySet( nWhichId, rItemSet, GetPropertySet(), m_bOverwriteLabelsForAttributedDataPointsAlso );
        }
        break;

        case SID_ATTR_NUMBERFORMAT_SOURCE:
        case SCHATTR_PERCENT_NUMBERFORMAT_SOURCE: //fall through intended
        {
            bChanged = lcl_UseSourceFormatFromItemToPropertySet( nWhichId, rItemSet, GetPropertySet(), m_bOverwriteLabelsForAttributedDataPointsAlso );
        }
        break;

        case SCHATTR_DATADESCR_SEPARATOR:
        {
            OUString aNewValue = static_cast< const SfxStringItem & >( rItemSet.Get( nWhichId )).GetValue();
            OUString aOldValue;
            try
            {
                GetPropertySet()->getPropertyValue( "LabelSeparator" ) >>= aOldValue;
                if( m_bOverwriteLabelsForAttributedDataPointsAlso )
                {
                    Reference< chart2::XDataSeries > xSeries( GetPropertySet(), uno::UNO_QUERY);
                    if( !aOldValue.equals(aNewValue) ||
                        DataSeriesHelper::hasAttributedDataPointDifferentValue( xSeries, "LabelSeparator" , uno::makeAny( aOldValue ) ) )
                    {
                        DataSeriesHelper::setPropertyAlsoToAllAttributedDataPoints( xSeries, "LabelSeparator" , uno::makeAny( aNewValue ) );
                        bChanged = true;
                    }
                }
                else if( !aOldValue.equals(aNewValue) )
                {
                    GetPropertySet()->setPropertyValue( "LabelSeparator" , uno::makeAny( aNewValue ));
                    bChanged = true;
                }
            }
            catch( const uno::Exception& e )
            {
                ASSERT_EXCEPTION( e );
            }
        }
        break;

        case SCHATTR_DATADESCR_PLACEMENT:
        {

            try
            {
                sal_Int32 nNew = static_cast< const SfxInt32Item & >( rItemSet.Get( nWhichId )).GetValue();
                sal_Int32 nOld =0;
                if( !(GetPropertySet()->getPropertyValue( "LabelPlacement" ) >>= nOld) )
                {
                    if( m_aAvailableLabelPlacements.getLength() )
                        nOld = m_aAvailableLabelPlacements[0];
                }
                if( m_bOverwriteLabelsForAttributedDataPointsAlso )
                {
                    Reference< chart2::XDataSeries > xSeries( GetPropertySet(), uno::UNO_QUERY);
                    if( nOld!=nNew ||
                        DataSeriesHelper::hasAttributedDataPointDifferentValue( xSeries, "LabelPlacement" , uno::makeAny( nOld ) ) )
                    {
                        DataSeriesHelper::setPropertyAlsoToAllAttributedDataPoints( xSeries, "LabelPlacement" , uno::makeAny( nNew ) );
                        bChanged = true;
                    }
                }
                else if( nOld!=nNew )
                {
                    GetPropertySet()->setPropertyValue( "LabelPlacement" , uno::makeAny( nNew ));
                    bChanged = true;
                }
            }
            catch( const uno::Exception& e )
            {
                ASSERT_EXCEPTION( e );
            }
        }
        break;

        case SCHATTR_STYLE_SYMBOL:
        {
            sal_Int32 nStyle =
                static_cast< const SfxInt32Item & >(
                    rItemSet.Get( nWhichId )).GetValue();
            chart2::Symbol aSymbol;

            GetPropertySet()->getPropertyValue( "Symbol" ) >>= aSymbol;
            sal_Int32 nOldStyle = lcl_getSymbolStyleForSymbol( aSymbol );

            if( nStyle != nOldStyle )
            {
                bool bDeleteSymbol = false;
                switch( nStyle )
                {
                    case SVX_SYMBOLTYPE_NONE:
                        aSymbol.Style = chart2::SymbolStyle_NONE;
                        break;
                    case SVX_SYMBOLTYPE_AUTO:
                        aSymbol.Style = chart2::SymbolStyle_AUTO;
                        break;
                    case SVX_SYMBOLTYPE_BRUSHITEM:
                        aSymbol.Style = chart2::SymbolStyle_GRAPHIC;
                        break;
                    case SVX_SYMBOLTYPE_UNKNOWN:
                        bDeleteSymbol = true;
                        break;

                    default:
                        aSymbol.Style = chart2::SymbolStyle_STANDARD;
                        aSymbol.StandardSymbol = nStyle;
                }

                if( bDeleteSymbol )
                    GetPropertySet()->setPropertyValue( "Symbol" , uno::Any());
                else
                    GetPropertySet()->setPropertyValue( "Symbol" , uno::makeAny( aSymbol ));
                bChanged = true;
            }
        }
        break;

        case SCHATTR_SYMBOL_SIZE:
        {
            Size aSize = static_cast< const SvxSizeItem & >(
                rItemSet.Get( nWhichId )).GetSize();
            chart2::Symbol aSymbol;

            GetPropertySet()->getPropertyValue( "Symbol" ) >>= aSymbol;
            if( aSize.getWidth() != aSymbol.Size.Width ||
                aSize.getHeight() != aSymbol.Size.Height )
            {
                aSymbol.Size.Width = aSize.getWidth();
                aSymbol.Size.Height = aSize.getHeight();

                GetPropertySet()->setPropertyValue( "Symbol" , uno::makeAny( aSymbol ));
                bChanged = true;
            }
        }
        break;

        case SCHATTR_SYMBOL_BRUSH:
        {
            const SvxBrushItem & rBrshItem( static_cast< const SvxBrushItem & >(
                                                rItemSet.Get( nWhichId )));
            uno::Any aXGraphicAny;
            const Graphic *pGraphic( rBrshItem.GetGraphic());
            if( pGraphic )
            {
                uno::Reference< graphic::XGraphic > xGraphic( pGraphic->GetXGraphic());
                if( xGraphic.is())
                {
                    aXGraphicAny <<= xGraphic;
                    chart2::Symbol aSymbol;
                    GetPropertySet()->getPropertyValue( "Symbol" ) >>= aSymbol;
                    if( aSymbol.Graphic != xGraphic )
                    {
                        aSymbol.Graphic = xGraphic;
                        GetPropertySet()->setPropertyValue( "Symbol" , uno::makeAny( aSymbol ));
                        bChanged = true;
                    }
                }
            }
        }
        break;

        case SCHATTR_TEXT_DEGREES:
        {
            double fValue = static_cast< double >(
                static_cast< const SfxInt32Item & >(
                    rItemSet.Get( nWhichId )).GetValue()) / 100.0;
            double fOldValue = 0.0;
            bool bPropExisted =
                ( GetPropertySet()->getPropertyValue( "TextRotation" ) >>= fOldValue );

            if( ! bPropExisted ||
                ( bPropExisted && fOldValue != fValue ))
            {
                GetPropertySet()->setPropertyValue( "TextRotation" , uno::makeAny( fValue ));
                bChanged = true;
            }
        }
        break;
    }

    return bChanged;
}

void DataPointItemConverter::FillSpecialItem(
    sal_uInt16 nWhichId, SfxItemSet & rOutItemSet ) const
    throw( uno::Exception )
{
    switch( nWhichId )
    {
        case SCHATTR_DATADESCR_SHOW_NUMBER:
        case SCHATTR_DATADESCR_SHOW_PERCENTAGE:
        case SCHATTR_DATADESCR_SHOW_CATEGORY:
        case SCHATTR_DATADESCR_SHOW_SYMBOL:
        {
            chart2::DataPointLabel aLabel;
            if (GetPropertySet()->getPropertyValue(CHART_UNONAME_LABEL) >>= aLabel)
            {
                bool bValue = (SCHATTR_DATADESCR_SHOW_NUMBER==nWhichId) ? aLabel.ShowNumber : (
                    (SCHATTR_DATADESCR_SHOW_PERCENTAGE==nWhichId) ? aLabel.ShowNumberInPercent : (
                    (SCHATTR_DATADESCR_SHOW_CATEGORY==nWhichId) ? aLabel.ShowCategoryName : aLabel.ShowLegendSymbol ));

                rOutItemSet.Put( SfxBoolItem( nWhichId, bValue ));

                if( m_bOverwriteLabelsForAttributedDataPointsAlso )
                {
                    if( DataSeriesHelper::hasAttributedDataPointDifferentValue(
                        Reference< chart2::XDataSeries >( GetPropertySet(), uno::UNO_QUERY), CHART_UNONAME_LABEL , uno::makeAny(aLabel) ) )
                    {
                        rOutItemSet.InvalidateItem(nWhichId);
                    }
                }
            }
        }
        break;

        case SID_ATTR_NUMBERFORMAT_VALUE:
        {
            sal_Int32 nKey = 0;
            if (!(GetPropertySet()->getPropertyValue(CHART_UNONAME_NUMFMT) >>= nKey))
                nKey = m_nNumberFormat;
            rOutItemSet.Put( SfxUInt32Item( nWhichId, nKey ));
        }
        break;

        case SCHATTR_PERCENT_NUMBERFORMAT_VALUE:
        {
            sal_Int32 nKey = 0;
            if( !(GetPropertySet()->getPropertyValue( "PercentageNumberFormat" ) >>= nKey) )
                nKey = m_nPercentNumberFormat;
            rOutItemSet.Put( SfxUInt32Item( nWhichId, nKey ));
        }
        break;

        case SID_ATTR_NUMBERFORMAT_SOURCE:
        {
            bool bNumberFormatIsSet = GetPropertySet()->getPropertyValue(CHART_UNONAME_NUMFMT).hasValue();
            rOutItemSet.Put( SfxBoolItem( nWhichId, ! bNumberFormatIsSet ));
        }
        break;
        case SCHATTR_PERCENT_NUMBERFORMAT_SOURCE:
        {
            bool bNumberFormatIsSet = ( GetPropertySet()->getPropertyValue( "PercentageNumberFormat" ).hasValue());
            rOutItemSet.Put( SfxBoolItem( nWhichId, ! bNumberFormatIsSet ));
        }
        break;

        case SCHATTR_DATADESCR_SEPARATOR:
        {
            OUString aValue;
            try
            {
                GetPropertySet()->getPropertyValue( "LabelSeparator" ) >>= aValue;
                rOutItemSet.Put( SfxStringItem( nWhichId, aValue ));
            }
            catch( const uno::Exception& e )
            {
                ASSERT_EXCEPTION( e );
            }
        }
        break;

        case SCHATTR_DATADESCR_PLACEMENT:
        {
            try
            {
                sal_Int32 nPlacement=0;
                if( GetPropertySet()->getPropertyValue( "LabelPlacement" ) >>= nPlacement )
                    rOutItemSet.Put( SfxInt32Item( nWhichId, nPlacement ));
                else if( m_aAvailableLabelPlacements.getLength() )
                    rOutItemSet.Put( SfxInt32Item( nWhichId, m_aAvailableLabelPlacements[0] ));
            }
            catch( const uno::Exception& e )
            {
                ASSERT_EXCEPTION( e );
            }
        }
        break;

        case SCHATTR_DATADESCR_AVAILABLE_PLACEMENTS:
        {
            rOutItemSet.Put( SfxIntegerListItem( nWhichId, m_aAvailableLabelPlacements ) );
        }
        break;

        case SCHATTR_DATADESCR_NO_PERCENTVALUE:
        {
            rOutItemSet.Put( SfxBoolItem( nWhichId, m_bForbidPercentValue ));
        }
        break;

        case SCHATTR_STYLE_SYMBOL:
        {
            chart2::Symbol aSymbol;
            if( GetPropertySet()->getPropertyValue( "Symbol" ) >>= aSymbol )
                rOutItemSet.Put( SfxInt32Item( nWhichId, lcl_getSymbolStyleForSymbol( aSymbol ) ));
        }
        break;

        case SCHATTR_SYMBOL_SIZE:
        {
            chart2::Symbol aSymbol;
            if( GetPropertySet()->getPropertyValue( "Symbol" ) >>= aSymbol )
                rOutItemSet.Put(
                    SvxSizeItem( nWhichId, Size( aSymbol.Size.Width, aSymbol.Size.Height ) ));
        }
        break;

        case SCHATTR_SYMBOL_BRUSH:
        {
            chart2::Symbol aSymbol;
            if(( GetPropertySet()->getPropertyValue( "Symbol" ) >>= aSymbol )
               && aSymbol.Graphic.is() )
            {
                rOutItemSet.Put( SvxBrushItem( Graphic( aSymbol.Graphic ), GPOS_MM, SCHATTR_SYMBOL_BRUSH ));
            }
        }
        break;

        case SCHATTR_TEXT_DEGREES:
        {
            double fValue = 0;

            if( GetPropertySet()->getPropertyValue( "TextRotation" ) >>= fValue )
            {
                rOutItemSet.Put( SfxInt32Item( nWhichId, static_cast< sal_Int32 >(
                                                   ::rtl::math::round( fValue * 100.0 ) ) ));
            }
        }
        break;
   }
}

}}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
