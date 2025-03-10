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
#include <DomainMapperTableHandler.hxx>
#include <DomainMapper_Impl.hxx>
#include <StyleSheetTable.hxx>
#include <com/sun/star/beans/XPropertyState.hpp>
#include <com/sun/star/container/XEnumerationAccess.hpp>
#include <com/sun/star/table/TableBorderDistances.hpp>
#include <com/sun/star/table/TableBorder.hpp>
#include <com/sun/star/table/BorderLine2.hpp>
#include <com/sun/star/table/XCellRange.hpp>
#include <com/sun/star/text/HoriOrientation.hpp>
#include <com/sun/star/text/RelOrientation.hpp>
#include <com/sun/star/text/SizeType.hpp>
#include <com/sun/star/text/VertOrientation.hpp>
#include <com/sun/star/style/ParagraphAdjust.hpp>
#include <dmapperLoggers.hxx>
#include <TablePositionHandler.hxx>

#ifdef DEBUG_DOMAINMAPPER
#include <PropertyMapHelper.hxx>
#include <rtl/ustring.hxx>
#endif

namespace writerfilter {
namespace dmapper {

using namespace ::com::sun::star;
using namespace ::std;

#define DEF_BORDER_DIST 190  //0,19cm

DomainMapperTableHandler::DomainMapperTableHandler(TextReference_t const& xText,
            DomainMapper_Impl& rDMapper_Impl)
    : m_xText(xText),
        m_rDMapper_Impl( rDMapper_Impl ),
        m_nCellIndex(0),
        m_nRowIndex(0)
{
}

DomainMapperTableHandler::~DomainMapperTableHandler()
{
}

void DomainMapperTableHandler::startTable(unsigned int nRows,
                                          unsigned int /*nDepth*/,
                                          TablePropertyMapPtr pProps)
{
    m_aTableProperties = pProps;
    m_pTableSeq = TableSequencePointer_t(new TableSequence_t(nRows));
    m_nRowIndex = 0;

#ifdef DEBUG_DOMAINMAPPER
    dmapper_logger->startElement("tablehandler.table");
    dmapper_logger->attribute("rows", nRows);

    if (pProps.get() != NULL)
        pProps->dumpXml( dmapper_logger );
#endif
}



PropertyMapPtr lcl_SearchParentStyleSheetAndMergeProperties(const StyleSheetEntryPtr pStyleSheet, StyleSheetTablePtr pStyleSheetTable)
{
    PropertyMapPtr pRet;
    if(!pStyleSheet->sBaseStyleIdentifier.isEmpty())
    {
        const StyleSheetEntryPtr pParentStyleSheet = pStyleSheetTable->FindStyleSheetByISTD( pStyleSheet->sBaseStyleIdentifier );
        pRet = lcl_SearchParentStyleSheetAndMergeProperties( pParentStyleSheet, pStyleSheetTable );
    }
    else
    {
        pRet.reset( new PropertyMap );
    }

    pRet->InsertProps(pStyleSheet->pProperties);

    return pRet;
}

void lcl_mergeBorder( PropertyIds nId, PropertyMapPtr pOrig, PropertyMapPtr pDest )
{
    boost::optional<PropertyMap::Property> pOrigVal = pOrig->getProperty(nId);

    if ( pOrigVal )
    {
        pDest->Insert( nId, pOrigVal->second, false );
    }
}

void lcl_computeCellBorders( PropertyMapPtr pTableBorders, PropertyMapPtr pCellProps,
        sal_Int32 nCell, sal_Int32 nRow, bool bIsEndCol, bool bIsEndRow )
{
    boost::optional<PropertyMap::Property> pVerticalVal = pCellProps->getProperty(META_PROP_VERTICAL_BORDER);
    boost::optional<PropertyMap::Property> pHorizontalVal = pCellProps->getProperty(META_PROP_HORIZONTAL_BORDER);

    // Handle the vertical and horizontal borders
    uno::Any aVertProp;
    if ( !pVerticalVal)
    {
        pVerticalVal = pTableBorders->getProperty(META_PROP_VERTICAL_BORDER);
        if ( pVerticalVal )
            aVertProp = pVerticalVal->second;
    }
    else
    {
        aVertProp = pVerticalVal->second;
        pCellProps->Erase( pVerticalVal->first );
    }

    uno::Any aHorizProp;
    if ( !pHorizontalVal )
    {
        pHorizontalVal = pTableBorders->getProperty(META_PROP_HORIZONTAL_BORDER);
        if ( pHorizontalVal )
            aHorizProp = pHorizontalVal->second;
    }
    else
    {
        aHorizProp = pHorizontalVal->second;
        pCellProps->Erase( pHorizontalVal->first );
    }

    if ( nCell == 0 )
    {
        lcl_mergeBorder( PROP_LEFT_BORDER, pTableBorders, pCellProps );
        if ( pVerticalVal )
            pCellProps->Insert( PROP_RIGHT_BORDER, aVertProp, false );
    }

    if ( bIsEndCol )
    {
        lcl_mergeBorder( PROP_RIGHT_BORDER, pTableBorders, pCellProps );
        if ( pVerticalVal )
            pCellProps->Insert( PROP_LEFT_BORDER, aVertProp, false );
    }

    if ( nCell > 0 && !bIsEndCol )
    {
        if ( pVerticalVal )
        {
            pCellProps->Insert( PROP_RIGHT_BORDER, aVertProp, false );
            pCellProps->Insert( PROP_LEFT_BORDER, aVertProp, false );
        }
    }

    if ( nRow == 0 )
    {
        lcl_mergeBorder( PROP_TOP_BORDER, pTableBorders, pCellProps );
        if ( pHorizontalVal )
            pCellProps->Insert( PROP_BOTTOM_BORDER, aHorizProp, false );
    }

    if ( bIsEndRow )
    {
        lcl_mergeBorder( PROP_BOTTOM_BORDER, pTableBorders, pCellProps );
        if ( pHorizontalVal )
            pCellProps->Insert( PROP_TOP_BORDER, aHorizProp, false );
    }

    if ( nRow > 0 && !bIsEndRow )
    {
        if ( pHorizontalVal )
        {
            pCellProps->Insert( PROP_TOP_BORDER, aHorizProp, false );
            pCellProps->Insert( PROP_BOTTOM_BORDER, aHorizProp, false );
        }
    }
}

#ifdef DEBUG_DOMAINMAPPER

void lcl_debug_BorderLine(table::BorderLine & rLine)
{
    dmapper_logger->startElement("BorderLine");
    dmapper_logger->attribute("Color", rLine.Color);
    dmapper_logger->attribute("InnerLineWidth", rLine.InnerLineWidth);
    dmapper_logger->attribute("OuterLineWidth", rLine.OuterLineWidth);
    dmapper_logger->attribute("LineDistance", rLine.LineDistance);
    dmapper_logger->endElement();
}

void lcl_debug_TableBorder(table::TableBorder & rBorder)
{
    dmapper_logger->startElement("TableBorder");
    lcl_debug_BorderLine(rBorder.TopLine);
    dmapper_logger->attribute("IsTopLineValid", rBorder.IsTopLineValid);
    lcl_debug_BorderLine(rBorder.BottomLine);
    dmapper_logger->attribute("IsBottomLineValid", rBorder.IsBottomLineValid);
    lcl_debug_BorderLine(rBorder.LeftLine);
    dmapper_logger->attribute("IsLeftLineValid", rBorder.IsLeftLineValid);
    lcl_debug_BorderLine(rBorder.RightLine);
    dmapper_logger->attribute("IsRightLineValid", rBorder.IsRightLineValid);
    lcl_debug_BorderLine(rBorder.VerticalLine);
    dmapper_logger->attribute("IsVerticalLineValid", rBorder.IsVerticalLineValid);
    lcl_debug_BorderLine(rBorder.HorizontalLine);
    dmapper_logger->attribute("IsHorizontalLineValid", rBorder.IsHorizontalLineValid);
    dmapper_logger->attribute("Distance", rBorder.Distance);
    dmapper_logger->attribute("IsDistanceValid", rBorder.IsDistanceValid);
    dmapper_logger->endElement();
}
#endif

struct TableInfo
{
    sal_Int32 nLeftBorderDistance;
    sal_Int32 nRightBorderDistance;
    sal_Int32 nTopBorderDistance;
    sal_Int32 nBottomBorderDistance;
    sal_Int32 nTblLook;
    sal_Int32 nNestLevel;
    PropertyMapPtr pTableDefaults;
    PropertyMapPtr pTableBorders;
    TableStyleSheetEntry* pTableStyle;
    TablePropertyValues_t aTableProperties;

    TableInfo()
    : nLeftBorderDistance(DEF_BORDER_DIST)
    , nRightBorderDistance(DEF_BORDER_DIST)
    , nTopBorderDistance(0)
    , nBottomBorderDistance(0)
    , nTblLook(0x4a0)
    , nNestLevel(0)
    , pTableDefaults(new PropertyMap)
    , pTableBorders(new PropertyMap)
    , pTableStyle(NULL)
    {
    }

};

namespace
{

bool lcl_extractTableBorderProperty(PropertyMapPtr pTableProperties, const PropertyIds nId, TableInfo& rInfo, table::BorderLine2& rLine)
{
    const boost::optional<PropertyMap::Property> aTblBorder = pTableProperties->getProperty(nId);
    if( aTblBorder )
    {
        OSL_VERIFY(aTblBorder->second >>= rLine);

        rInfo.pTableBorders->Insert( nId, uno::makeAny( rLine ) );
        rInfo.pTableDefaults->Erase( nId );

        return true;
    }

    return false;
}

}

bool lcl_extractHoriOrient(uno::Sequence<beans::PropertyValue>& rFrameProperties, sal_Int32& nHoriOrient)
{
    // Shifts the frame left by the given value.
    for (sal_Int32 i = 0; i < rFrameProperties.getLength(); ++i)
    {
        if (rFrameProperties[i].Name == "HoriOrient")
        {
            nHoriOrient = rFrameProperties[i].Value.get<sal_Int32>();
            return true;
        }
    }
    return false;
}

void lcl_DecrementHoriOrientPosition(uno::Sequence<beans::PropertyValue>& rFrameProperties, sal_Int32 nAmount)
{
    // Shifts the frame left by the given value.
    for (sal_Int32 i = 0; i < rFrameProperties.getLength(); ++i)
    {
        beans::PropertyValue& rPropertyValue = rFrameProperties[i];
        if (rPropertyValue.Name == "HoriOrientPosition")
        {
            sal_Int32 nValue = rPropertyValue.Value.get<sal_Int32>();
            nValue -= nAmount;
            rPropertyValue.Value <<= nValue;
            return;
        }
    }
}

TableStyleSheetEntry * DomainMapperTableHandler::endTableGetTableStyle(TableInfo & rInfo, uno::Sequence<beans::PropertyValue>& rFrameProperties)
{
    // will receive the table style if any
    TableStyleSheetEntry* pTableStyle = NULL;

    if( m_aTableProperties.get() )
    {
        //create properties from the table attributes
        //...pPropMap->Insert( PROP_LEFT_MARGIN, uno::makeAny( m_nLeftMargin - m_nGapHalf ));
        //pPropMap->Insert( PROP_HORI_ORIENT, uno::makeAny( text::HoriOrientation::RIGHT ));
        sal_Int32 nGapHalf = 0;
        sal_Int32 nLeftMargin = 0;
        sal_Int32 nTableWidth = 0;
        sal_Int32 nTableWidthType = text::SizeType::FIX;

        comphelper::SequenceAsHashMap aGrabBag;

        if (0 != m_rDMapper_Impl.getTableManager().getCurrentTableRealPosition())
        {
            TablePositionHandler *pTablePositions = m_rDMapper_Impl.getTableManager().getCurrentTableRealPosition();

            uno::Sequence< beans::PropertyValue  > aGrabBagTS( 10 );

            aGrabBagTS[0].Name = "bottomFromText";
            aGrabBagTS[0].Value = uno::makeAny(pTablePositions->getBottomFromText() );

            aGrabBagTS[1].Name = "horzAnchor";
            aGrabBagTS[1].Value = uno::makeAny( pTablePositions->getHorzAnchor() );

            aGrabBagTS[2].Name = "leftFromText";
            aGrabBagTS[2].Value = uno::makeAny( pTablePositions->getLeftFromText() );

            aGrabBagTS[3].Name = "rightFromText";
            aGrabBagTS[3].Value = uno::makeAny( pTablePositions->getRightFromText() );

            aGrabBagTS[4].Name = "tblpX";
            aGrabBagTS[4].Value = uno::makeAny( pTablePositions->getX() );

            aGrabBagTS[5].Name = "tblpXSpec";
            aGrabBagTS[5].Value = uno::makeAny( pTablePositions->getXSpec() );

            aGrabBagTS[6].Name = "tblpY";
            aGrabBagTS[6].Value = uno::makeAny( pTablePositions->getY() );

            aGrabBagTS[7].Name = "tblpYSpec";
            aGrabBagTS[7].Value = uno::makeAny( pTablePositions->getYSpec() );

            aGrabBagTS[8].Name = "topFromText";
            aGrabBagTS[8].Value = uno::makeAny( pTablePositions->getTopFromText() );

            aGrabBagTS[9].Name = "vertAnchor";
            aGrabBagTS[9].Value = uno::makeAny( pTablePositions->getVertAnchor() );

            aGrabBag["TablePosition"] = uno::makeAny( aGrabBagTS );
        }

        boost::optional<PropertyMap::Property> aTableStyleVal = m_aTableProperties->getProperty(META_PROP_TABLE_STYLE_NAME);
        if(aTableStyleVal)
        {
            // Apply table style properties recursively
            OUString sTableStyleName;
            aTableStyleVal->second >>= sTableStyleName;
            StyleSheetTablePtr pStyleSheetTable = m_rDMapper_Impl.GetStyleSheetTable();
            const StyleSheetEntryPtr pStyleSheet = pStyleSheetTable->FindStyleSheetByISTD( sTableStyleName );
            pTableStyle = dynamic_cast<TableStyleSheetEntry*>( pStyleSheet.get( ) );
            m_aTableProperties->Erase( aTableStyleVal->first );

            aGrabBag["TableStyleName"] = uno::makeAny( sTableStyleName );

            if( pStyleSheet )
            {
                // First get the style properties, then the table ones
                PropertyMapPtr pTableProps( m_aTableProperties );
                TablePropertyMapPtr pEmptyProps( new TablePropertyMap );

                m_aTableProperties = pEmptyProps;

                PropertyMapPtr pMergedProperties = lcl_SearchParentStyleSheetAndMergeProperties(pStyleSheet, pStyleSheetTable);

                table::BorderLine2 aBorderLine;
                TableInfo rStyleInfo;
                if (lcl_extractTableBorderProperty(pMergedProperties, PROP_TOP_BORDER, rStyleInfo, aBorderLine))
                {
                    aGrabBag["TableStyleTopBorder"] = uno::makeAny( aBorderLine );
                }
                if (lcl_extractTableBorderProperty(pMergedProperties, PROP_BOTTOM_BORDER, rStyleInfo, aBorderLine))
                {
                    aGrabBag["TableStyleBottomBorder"] = uno::makeAny( aBorderLine );
                }
                if (lcl_extractTableBorderProperty(pMergedProperties, PROP_LEFT_BORDER, rStyleInfo, aBorderLine))
                {
                    aGrabBag["TableStyleLeftBorder"] = uno::makeAny( aBorderLine );
                }
                if (lcl_extractTableBorderProperty(pMergedProperties, PROP_RIGHT_BORDER, rStyleInfo, aBorderLine))
                {
                    aGrabBag["TableStyleRightBorder"] = uno::makeAny( aBorderLine );
                }

#ifdef DEBUG_DOMAINMAPPER
                dmapper_logger->startElement("mergedProps");
                pMergedProperties->dumpXml( dmapper_logger );
                dmapper_logger->endElement();
#endif

                m_aTableProperties->InsertProps(pMergedProperties);
                m_aTableProperties->InsertProps(pTableProps);

#ifdef DEBUG_DOMAINMAPPER
                dmapper_logger->startElement("TableProperties");
                m_aTableProperties->dumpXml( dmapper_logger );
                dmapper_logger->endElement();
#endif
            }
        }

        // This is the one preserving just all the table look attributes.
        boost::optional<PropertyMap::Property> oTableLook = m_aTableProperties->getProperty(META_PROP_TABLE_LOOK);
        if (oTableLook)
        {
            aGrabBag["TableStyleLook"] = oTableLook->second;
            m_aTableProperties->Erase(oTableLook->first);
        }

        // This is just the "val" attribute's numeric value.
        const boost::optional<PropertyMap::Property> aTblLook = m_aTableProperties->getProperty(PROP_TBL_LOOK);
        if(aTblLook)
        {
            aTblLook->second >>= rInfo.nTblLook;
            m_aTableProperties->Erase( aTblLook->first );
        }

        // Set the table default attributes for the cells
        rInfo.pTableDefaults->InsertProps(m_aTableProperties);

#ifdef DEBUG_DOMAINMAPPER
        dmapper_logger->startElement("TableDefaults");
        rInfo.pTableDefaults->dumpXml( dmapper_logger );
        dmapper_logger->endElement();
#endif

        if (!aGrabBag.empty())
        {
            m_aTableProperties->Insert( PROP_TABLE_INTEROP_GRAB_BAG, uno::makeAny( aGrabBag.getAsConstPropertyValueList() ) );
        }

        m_aTableProperties->getValue( TablePropertyMap::GAP_HALF, nGapHalf );
        m_aTableProperties->getValue( TablePropertyMap::LEFT_MARGIN, nLeftMargin );

        m_aTableProperties->getValue( TablePropertyMap::CELL_MAR_LEFT,
                                     rInfo.nLeftBorderDistance );
        m_aTableProperties->getValue( TablePropertyMap::CELL_MAR_RIGHT,
                                     rInfo.nRightBorderDistance );
        m_aTableProperties->getValue( TablePropertyMap::CELL_MAR_TOP,
                                     rInfo.nTopBorderDistance );
        m_aTableProperties->getValue( TablePropertyMap::CELL_MAR_BOTTOM,
                                     rInfo.nBottomBorderDistance );

        table::TableBorderDistances aDistances;
        aDistances.IsTopDistanceValid =
        aDistances.IsBottomDistanceValid =
        aDistances.IsLeftDistanceValid =
        aDistances.IsRightDistanceValid = sal_True;
        aDistances.TopDistance = static_cast<sal_Int16>( rInfo.nTopBorderDistance );
        aDistances.BottomDistance = static_cast<sal_Int16>( rInfo.nBottomBorderDistance );
        aDistances.LeftDistance = static_cast<sal_Int16>( rInfo.nLeftBorderDistance );
        aDistances.RightDistance = static_cast<sal_Int16>( rInfo.nRightBorderDistance );

        m_aTableProperties->Insert( PROP_TABLE_BORDER_DISTANCES, uno::makeAny( aDistances ) );

        if (rFrameProperties.hasElements())
            lcl_DecrementHoriOrientPosition(rFrameProperties, rInfo.nLeftBorderDistance);

        // Set table above/bottom spacing to 0.
        m_aTableProperties->Insert( PROP_TOP_MARGIN, uno::makeAny( sal_Int32( 0 ) ) );
        m_aTableProperties->Insert( PROP_BOTTOM_MARGIN, uno::makeAny( sal_Int32( 0 ) ) );

        //table border settings
        table::TableBorder aTableBorder;
        table::BorderLine2 aBorderLine, aLeftBorder;

        if (lcl_extractTableBorderProperty(m_aTableProperties, PROP_TOP_BORDER, rInfo, aBorderLine))
        {
            aTableBorder.TopLine = aBorderLine;
            aTableBorder.IsTopLineValid = sal_True;
        }
        if (lcl_extractTableBorderProperty(m_aTableProperties, PROP_BOTTOM_BORDER, rInfo, aBorderLine))
        {
            aTableBorder.BottomLine = aBorderLine;
            aTableBorder.IsBottomLineValid = sal_True;
        }
        if (lcl_extractTableBorderProperty(m_aTableProperties, PROP_LEFT_BORDER, rInfo, aLeftBorder))
        {
            aTableBorder.LeftLine = aLeftBorder;
            aTableBorder.IsLeftLineValid = sal_True;
            // Only top level table position depends on border width
            if (rInfo.nNestLevel == 1)
            {
                if (!rFrameProperties.hasElements())
                    rInfo.nLeftBorderDistance += aLeftBorder.LineWidth * 0.5;
                else
                    lcl_DecrementHoriOrientPosition(rFrameProperties, aLeftBorder.LineWidth * 0.5);
            }
        }
        if (lcl_extractTableBorderProperty(m_aTableProperties, PROP_RIGHT_BORDER, rInfo, aBorderLine))
        {
            aTableBorder.RightLine = aBorderLine;
            aTableBorder.IsRightLineValid = sal_True;
        }
        if (lcl_extractTableBorderProperty(m_aTableProperties, META_PROP_HORIZONTAL_BORDER, rInfo, aBorderLine))
        {
            aTableBorder.HorizontalLine = aBorderLine;
            aTableBorder.IsHorizontalLineValid = sal_True;
        }
        if (lcl_extractTableBorderProperty(m_aTableProperties, META_PROP_VERTICAL_BORDER, rInfo, aBorderLine))
        {
            aTableBorder.VerticalLine = aBorderLine;
            aTableBorder.IsVerticalLineValid = sal_True;
        }

        aTableBorder.Distance = 0;
        aTableBorder.IsDistanceValid = sal_False;

        m_aTableProperties->Insert( PROP_TABLE_BORDER, uno::makeAny( aTableBorder ) );

#ifdef DEBUG_DOMAINMAPPER
        lcl_debug_TableBorder(aTableBorder);
#endif

        // Table position in Office is computed in 2 different ways :
        // - top level tables: the goal is to have in-cell text starting at table indent pos (tblInd),
        //   so table's position depends on table's cells margin
        // - nested tables: the goal is to have left-most border starting at table_indent pos
        if (rInfo.nNestLevel > 1)
        {
            m_aTableProperties->Insert( PROP_LEFT_MARGIN, uno::makeAny( nLeftMargin - nGapHalf ));
        }
        else
        {
            m_aTableProperties->Insert( PROP_LEFT_MARGIN, uno::makeAny( nLeftMargin - nGapHalf - rInfo.nLeftBorderDistance ));
        }

        m_aTableProperties->getValue( TablePropertyMap::TABLE_WIDTH, nTableWidth );
        m_aTableProperties->getValue( TablePropertyMap::TABLE_WIDTH_TYPE, nTableWidthType );
        if( nTableWidthType == text::SizeType::FIX )
        {
            if( nTableWidth > 0 )
                m_aTableProperties->Insert( PROP_WIDTH, uno::makeAny( nTableWidth ));
        }
        else
        {
            m_aTableProperties->Insert( PROP_RELATIVE_WIDTH, uno::makeAny( sal_Int16( nTableWidth ) ) );
            m_aTableProperties->Insert( PROP_IS_WIDTH_RELATIVE, uno::makeAny( true ) );
        }

        sal_Int32 nHoriOrient = text::HoriOrientation::LEFT_AND_WIDTH;
        // Fetch Horizontal Orientation in rFrameProperties if not set in m_aTableProperties
        if ( !m_aTableProperties->getValue( TablePropertyMap::HORI_ORIENT, nHoriOrient ) )
            lcl_extractHoriOrient( rFrameProperties, nHoriOrient );
        m_aTableProperties->Insert( PROP_HORI_ORIENT, uno::makeAny( sal_Int16(nHoriOrient) ) );
        //fill default value - if not available
        m_aTableProperties->Insert( PROP_HEADER_ROW_COUNT, uno::makeAny( (sal_Int32)0), false);

        rInfo.aTableProperties = m_aTableProperties->GetPropertyValues();

#ifdef DEBUG_DOMAINMAPPER
        dmapper_logger->startElement("debug.tableprops");
        m_aTableProperties->dumpXml( dmapper_logger );
        dmapper_logger->endElement();
#endif

    }

    return pTableStyle;
}

#define CNF_FIRST_ROW               0x800
#define CNF_LAST_ROW                0x400
#define CNF_FIRST_COLUMN            0x200
#define CNF_LAST_COLUMN             0x100
#define CNF_ODD_VBAND               0x080
#define CNF_EVEN_VBAND              0x040
#define CNF_ODD_HBAND               0x020
#define CNF_EVEN_HBAND              0x010
#define CNF_FIRST_ROW_LAST_COLUMN   0x008
#define CNF_FIRST_ROW_FIRST_COLUMN  0x004
#define CNF_LAST_ROW_LAST_COLUMN    0x002
#define CNF_LAST_ROW_FIRST_COLUMN   0x001

CellPropertyValuesSeq_t DomainMapperTableHandler::endTableGetCellProperties(TableInfo & rInfo, std::vector<HorizontallyMergedCell>& rMerges)
{
#ifdef DEBUG_DOMAINMAPPER
    dmapper_logger->startElement("getCellProperties");
#endif

    CellPropertyValuesSeq_t aCellProperties( m_aCellProperties.size() );

    if ( !m_aCellProperties.size() )
    {
        #ifdef DEBUG_DOMAINMAPPER
        dmapper_logger->endElement();
        #endif
        return aCellProperties;
    }
    // std::vector< std::vector<PropertyMapPtr> > m_aCellProperties
    PropertyMapVector2::const_iterator aRowOfCellsIterator = m_aCellProperties.begin();
    PropertyMapVector2::const_iterator aRowOfCellsIteratorEnd = m_aCellProperties.end();
    PropertyMapVector2::const_iterator aLastRowIterator = m_aCellProperties.end() - 1;
    sal_Int32 nRow = 0;

    //it's a uno::Sequence< beans::PropertyValues >*
    RowPropertyValuesSeq_t* pCellProperties = aCellProperties.getArray();
    PropertyMapVector1::const_iterator aRowIter = m_aRowProperties.begin();
    while( aRowOfCellsIterator != aRowOfCellsIteratorEnd )
    {
        //aRowOfCellsIterator points to a vector of PropertyMapPtr
        PropertyMapVector1::const_iterator aCellIterator = aRowOfCellsIterator->begin();
        PropertyMapVector1::const_iterator aCellIteratorEnd = aRowOfCellsIterator->end();

        sal_Int32 nRowStyleMask = 0;

        if (aRowOfCellsIterator==m_aCellProperties.begin())
        {
            if(rInfo.nTblLook&0x20)
                nRowStyleMask |= CNF_FIRST_ROW;     // first row style used
        }
        else if (aRowOfCellsIterator==aLastRowIterator)
        {
            if(rInfo.nTblLook&0x40)
                nRowStyleMask |= CNF_LAST_ROW;      // last row style used
        }
        else if (*aRowIter && (*aRowIter)->isSet(PROP_TBL_HEADER))
            nRowStyleMask |= CNF_FIRST_ROW; // table header implies first row
        if(!nRowStyleMask)                          // if no row style used yet
        {
            // banding used only if not first and or last row style used
            if(!(rInfo.nTblLook&0x200))
            {   // hbanding used
                int n = nRow + 1;
                if(rInfo.nTblLook&0x20)
                    n++;
                if(n & 1)
                    nRowStyleMask = CNF_ODD_HBAND;
                else
                    nRowStyleMask = CNF_EVEN_HBAND;
            }
        }

        sal_Int32 nCell = 0;
        pCellProperties[nRow].realloc( aRowOfCellsIterator->size() );
        beans::PropertyValues* pSingleCellProperties = pCellProperties[nRow].getArray();
        while( aCellIterator != aCellIteratorEnd )
        {
            PropertyMapPtr pAllCellProps( new PropertyMap );

            PropertyMapVector1::const_iterator aLastCellIterator = aRowOfCellsIterator->end() - 1;
            bool bIsEndCol = aCellIterator == aLastCellIterator;
            bool bIsEndRow = aRowOfCellsIterator == aLastRowIterator;

            //aCellIterator points to a PropertyMapPtr;
            if( *aCellIterator )
            {
                pAllCellProps->InsertProps(rInfo.pTableDefaults);

                sal_Int32 nCellStyleMask = 0;
                if (aCellIterator==aRowOfCellsIterator->begin())
                {
                    if(rInfo.nTblLook&0x80)
                        nCellStyleMask = CNF_FIRST_COLUMN;      // first col style used
                }
                else if (bIsEndCol)
                {
                    if(rInfo.nTblLook&0x100)
                        nCellStyleMask = CNF_LAST_COLUMN;       // last col style used
                }
                if(!nCellStyleMask)                 // if no cell style is used yet
                {
                    if(!(rInfo.nTblLook&0x400))
                    {   // vbanding used
                        int n = nCell + 1;
                        if(rInfo.nTblLook&0x80)
                            n++;
                        if(n & 1)
                            nCellStyleMask = CNF_ODD_VBAND;
                        else
                            nCellStyleMask = CNF_EVEN_VBAND;
                    }
                }
                sal_Int32 nCnfStyleMask = nCellStyleMask + nRowStyleMask;
                if(nCnfStyleMask == CNF_FIRST_COLUMN + CNF_FIRST_ROW)
                    nCnfStyleMask |= CNF_FIRST_ROW_FIRST_COLUMN;
                else if(nCnfStyleMask == CNF_FIRST_COLUMN + CNF_LAST_ROW)
                    nCnfStyleMask |= CNF_LAST_ROW_FIRST_COLUMN;
                else if(nCnfStyleMask == CNF_LAST_COLUMN + CNF_FIRST_ROW)
                    nCnfStyleMask |= CNF_FIRST_ROW_LAST_COLUMN;
                else if(nCnfStyleMask == CNF_LAST_COLUMN + CNF_LAST_ROW)
                    nCnfStyleMask |= CNF_LAST_ROW_LAST_COLUMN;

                if ( rInfo.pTableStyle )
                {
                    PropertyMapPtr pStyleProps = rInfo.pTableStyle->GetProperties( nCnfStyleMask );
                    pAllCellProps->InsertProps( pStyleProps );
                }

                // Remove properties from style/row that aren't allowed in cells
                pAllCellProps->Erase( PROP_HEADER_ROW_COUNT );
                pAllCellProps->Erase( PROP_TBL_HEADER );

                // Then add the cell properties
                pAllCellProps->InsertProps(*aCellIterator);
                std::swap(*(*aCellIterator), *pAllCellProps );

#ifdef DEBUG_DOMAINMAPPER
                dmapper_logger->startElement("cell");
                dmapper_logger->attribute("cell", nCell);
                dmapper_logger->attribute("row", nRow);
#endif

                lcl_computeCellBorders( rInfo.pTableBorders, *aCellIterator, nCell, nRow, bIsEndCol, bIsEndRow );

                //now set the default left+right border distance TODO: there's an sprm containing the default distance!
                aCellIterator->get()->Insert( PROP_LEFT_BORDER_DISTANCE,
                                                 uno::makeAny(rInfo.nLeftBorderDistance ), false);
                aCellIterator->get()->Insert( PROP_RIGHT_BORDER_DISTANCE,
                                                 uno::makeAny((sal_Int32) rInfo.nRightBorderDistance ), false);
                aCellIterator->get()->Insert( PROP_TOP_BORDER_DISTANCE,
                                                 uno::makeAny((sal_Int32) rInfo.nTopBorderDistance ), false);
                aCellIterator->get()->Insert( PROP_BOTTOM_BORDER_DISTANCE,
                                                 uno::makeAny((sal_Int32) rInfo.nBottomBorderDistance ), false);

                // Horizontal merge is not an UNO property, extract that info here to rMerges, and then remove it from the map.
                const boost::optional<PropertyMap::Property> aHorizontalMergeVal = (*aCellIterator)->getProperty(PROP_HORIZONTAL_MERGE);
                if (aHorizontalMergeVal)
                {
                    if (aHorizontalMergeVal->second.get<sal_Bool>())
                    {
                        // first cell in a merge
                        HorizontallyMergedCell aMerge(nRow, nCell);
                        rMerges.push_back(aMerge);
                    }
                    else if (!rMerges.empty())
                    {
                        // resuming an earlier merge
                        HorizontallyMergedCell& rMerge = rMerges.back();
                        rMerge.m_nLastRow = nRow;
                        rMerge.m_nLastCol = nCell;
                    }
                    (*aCellIterator)->Erase(PROP_HORIZONTAL_MERGE);
                }

                // Cell direction is not an UNO Property, either.
                const boost::optional<PropertyMap::Property> aCellDirectionVal = (*aCellIterator)->getProperty(PROP_CELL_DIRECTION);
                if (aCellDirectionVal)
                {
                    if (aCellDirectionVal->second.get<sal_Int32>() == 3)
                    {
                        // btLr, so map ParagraphAdjust_CENTER to VertOrientation::CENTER.
                        uno::Reference<beans::XPropertySet> xPropertySet((*m_pTableSeq)[nRow][nCell][0], uno::UNO_QUERY);
                        if (xPropertySet->getPropertyValue("ParaAdjust").get<sal_Int16>() == style::ParagraphAdjust_CENTER)
                            (*aCellIterator)->Insert(PROP_VERT_ORIENT, uno::makeAny(text::VertOrientation::CENTER));
                    }
                    (*aCellIterator)->Erase(PROP_CELL_DIRECTION);
                }

                pSingleCellProperties[nCell] = (*aCellIterator)->GetPropertyValues();
#ifdef DEBUG_DOMAINMAPPER
                dmapper_logger->endElement();
#endif
            }
            ++nCell;
            ++aCellIterator;
        }
#ifdef DEBUG_DOMAINMAPPER
        //-->debug cell properties
        {
            OUString sNames;
            const uno::Sequence< beans::PropertyValues > aDebugCurrentRow = aCellProperties[nRow];
            sal_Int32 nDebugCells = aDebugCurrentRow.getLength();
            (void) nDebugCells;
            for( sal_Int32  nDebugCell = 0; nDebugCell < nDebugCells; ++nDebugCell)
            {
                const uno::Sequence< beans::PropertyValue >& aDebugCellProperties = aDebugCurrentRow[nDebugCell];
                sal_Int32 nDebugCellProperties = aDebugCellProperties.getLength();
                for( sal_Int32  nDebugProperty = 0; nDebugProperty < nDebugCellProperties; ++nDebugProperty)
                {
                    const OUString sName = aDebugCellProperties[nDebugProperty].Name;
                    sNames += sName;
                    sNames += OUString('-');
                }
                sNames += OUString('\n');
            }
            (void)sNames;
        }
        //--<
#endif
        ++nRow;
        ++aRowOfCellsIterator;
        ++aRowIter;
    }

#ifdef DEBUG_DOMAINMAPPER
    dmapper_logger->endElement();
#endif

    return aCellProperties;
}

RowPropertyValuesSeq_t DomainMapperTableHandler::endTableGetRowProperties()
{
#ifdef DEBUG_DOMAINMAPPER
    dmapper_logger->startElement("getRowProperties");
#endif

    RowPropertyValuesSeq_t aRowProperties( m_aRowProperties.size() );
    PropertyMapVector1::const_iterator aRowIter = m_aRowProperties.begin();
    PropertyMapVector1::const_iterator aRowIterEnd = m_aRowProperties.end();
    sal_Int32 nRow = 0;
    while( aRowIter != aRowIterEnd )
    {
#ifdef DEBUG_DOMAINMAPPER
        dmapper_logger->startElement("rowProps.row");
#endif
        if( aRowIter->get() )
        {
            //set default to 'break across pages"
            (*aRowIter)->Insert( PROP_IS_SPLIT_ALLOWED, uno::makeAny(sal_True ), false );
            // tblHeader is only our property, remove before the property map hits UNO
            (*aRowIter)->Erase(PROP_TBL_HEADER);

            aRowProperties[nRow] = (*aRowIter)->GetPropertyValues();
#ifdef DEBUG_DOMAINMAPPER
            ((*aRowIter)->dumpXml( dmapper_logger ));
            lcl_DumpPropertyValues(dmapper_logger, aRowProperties[nRow]);
#endif
        }
        ++nRow;
        ++aRowIter;
#ifdef DEBUG_DOMAINMAPPER
        dmapper_logger->endElement();
#endif
    }

#ifdef DEBUG_DOMAINMAPPER
    dmapper_logger->endElement();
#endif

    return aRowProperties;
}

// Apply paragraph property to each paragraph within a cell.
static void lcl_ApplyCellParaProps(uno::Reference<table::XCell> const& xCell,
        uno::Any aBottomMargin)
{
    uno::Reference<container::XEnumerationAccess> xEnumerationAccess(xCell, uno::UNO_QUERY);
    uno::Reference<container::XEnumeration> xEnumeration = xEnumerationAccess->createEnumeration();
    while (xEnumeration->hasMoreElements())
    {
        uno::Reference<beans::XPropertySet> xParagraph(xEnumeration->nextElement(), uno::UNO_QUERY);
        uno::Reference<beans::XPropertyState> xPropertyState(xParagraph, uno::UNO_QUERY);
        // Don't apply in case direct formatting is already present.
        // TODO: probably paragraph style has priority over table style here.
        if (xPropertyState.is() && xPropertyState->getPropertyState("ParaBottomMargin") == beans::PropertyState_DEFAULT_VALUE)
            xParagraph->setPropertyValue("ParaBottomMargin", aBottomMargin);
    }
}

void DomainMapperTableHandler::endTable(unsigned int nestedTableLevel)
{
#ifdef DEBUG_DOMAINMAPPER
    dmapper_logger->startElement("tablehandler.endTable");
#endif

    // If we want to make this table a floating one.
    uno::Sequence<beans::PropertyValue> aFrameProperties = m_rDMapper_Impl.getTableManager().getCurrentTablePosition();
    TableInfo aTableInfo;
    aTableInfo.nNestLevel = nestedTableLevel;
    aTableInfo.pTableStyle = endTableGetTableStyle(aTableInfo, aFrameProperties);
    //  expands to uno::Sequence< Sequence< beans::PropertyValues > >

    std::vector<HorizontallyMergedCell> aMerges;
    CellPropertyValuesSeq_t aCellProperties = endTableGetCellProperties(aTableInfo, aMerges);

    RowPropertyValuesSeq_t aRowProperties = endTableGetRowProperties();

#ifdef DEBUG_DOMAINMAPPER
    lcl_DumpPropertyValueSeq(dmapper_logger, aRowProperties);
#endif

    if (m_pTableSeq->getLength() > 0)
    {
        uno::Reference<text::XTextRange> xStart;
        uno::Reference<text::XTextRange> xEnd;

        bool bFloating = aFrameProperties.hasElements();
        // Additional checks: if we can do this.
        if (bFloating && (*m_pTableSeq)[0].getLength() > 0 && (*m_pTableSeq)[0][0].getLength() > 0)
        {
            xStart = (*m_pTableSeq)[0][0][0];
            uno::Sequence< uno::Sequence< uno::Reference<text::XTextRange> > >& rLastRow = (*m_pTableSeq)[m_pTableSeq->getLength() - 1];
            uno::Sequence< uno::Reference<text::XTextRange> >& rLastCell = rLastRow[rLastRow.getLength() - 1];
            xEnd = rLastCell[1];
        }
        uno::Reference<text::XTextTable> xTable;
        try
        {
            if (m_xText.is())
            {
                xTable = m_xText->convertToTable(*m_pTableSeq,
                        aCellProperties,
                        aRowProperties,
                        aTableInfo.aTableProperties);

                if (xTable.is())
                {
                    m_xTableRange = xTable->getAnchor( );

                    if (!aMerges.empty())
                    {
                        // Perform horizontal merges in reverse order, so the fact that merging changes the position of cells won't cause a problem for us.
                        for (std::vector<HorizontallyMergedCell>::reverse_iterator it = aMerges.rbegin(); it != aMerges.rend(); ++it)
                        {
                            uno::Reference<table::XCellRange> xCellRange(xTable, uno::UNO_QUERY_THROW);
                            uno::Reference<beans::XPropertySet> xCell(xCellRange->getCellByPosition(it->m_nFirstCol, it->m_nFirstRow), uno::UNO_QUERY_THROW);
                            OUString aFirst = xCell->getPropertyValue("CellName").get<OUString>();
                            xCell.set(xCellRange->getCellByPosition(it->m_nLastCol, it->m_nLastRow), uno::UNO_QUERY_THROW);
                            OUString aLast = xCell->getPropertyValue("CellName").get<OUString>();

                            uno::Reference<text::XTextTableCursor> xCursor = xTable->createCursorByCellName(aFirst);
                            xCursor->gotoCellByName(aLast, true);
                            xCursor->mergeRange();
                        }
                    }
                }

                // OOXML table style may container paragraph properties, apply these now.
                for (int i = 0; i < aTableInfo.aTableProperties.getLength(); ++i)
                {
                    if (aTableInfo.aTableProperties[i].Name == "ParaBottomMargin")
                    {
                        uno::Reference<table::XCellRange> xCellRange(xTable, uno::UNO_QUERY);
                        uno::Any aBottomMargin = aTableInfo.aTableProperties[i].Value;
                        sal_Int32 nRows = aCellProperties.getLength();
                        for (sal_Int32 nRow = 0; nRow < nRows; ++nRow)
                        {
                            const uno::Sequence< beans::PropertyValues > aCurrentRow = aCellProperties[nRow];
                            sal_Int32 nCells = aCurrentRow.getLength();
                            for (sal_Int32 nCell = 0; nCell < nCells; ++nCell)
                                lcl_ApplyCellParaProps(xCellRange->getCellByPosition(nCell, nRow), aBottomMargin);
                        }
                        break;
                    }
                }
            }
        }
        catch ( const lang::IllegalArgumentException &e )
        {
            SAL_INFO("writerfilter.dmapper",
                    "Conversion to table error: " << e.Message);
#ifdef DEBUG_DOMAINMAPPER
            dmapper_logger->chars(std::string("failed to import table!"));
#endif
        }
        catch ( const uno::Exception &e )
        {
            SAL_INFO("writerfilter.dmapper",
                    "Exception during table creation: " << e.Message);
        }

        // If we have a table with a start and an end position, we should make it a floating one.
        if (xTable.is() && xStart.is() && xEnd.is())
        {
            uno::Reference<beans::XPropertySet> xTableProperties(xTable, uno::UNO_QUERY);
            bool bIsRelative = false;
            xTableProperties->getPropertyValue("IsWidthRelative") >>= bIsRelative;
            if (!bIsRelative)
            {
                aFrameProperties.realloc(aFrameProperties.getLength() + 1);
                aFrameProperties[aFrameProperties.getLength() - 1].Name = "Width";
                aFrameProperties[aFrameProperties.getLength() - 1].Value = xTableProperties->getPropertyValue("Width");
            }
            else
            {
                aFrameProperties.realloc(aFrameProperties.getLength() + 1);
                aFrameProperties[aFrameProperties.getLength() - 1].Name = "FrameWidthPercent";
                aFrameProperties[aFrameProperties.getLength() - 1].Value = xTableProperties->getPropertyValue("RelativeWidth");

                // Applying the relative width to the frame, needs to have the table width to be 100% of the frame width
                xTableProperties->setPropertyValue("RelativeWidth", uno::makeAny(sal_Int16(100)));
            }

            // A non-zero left margin would move the table out of the frame, move the frame itself instead.
            xTableProperties->setPropertyValue("LeftMargin", uno::makeAny(sal_Int32(0)));

            // In case the document ends with a table, we're called after
            // SectionPropertyMap::CloseSectionGroup(), so we'll have no idea
            // about the text area width, nor can fix this by delaying the text
            // frame conversion: just do it here.
            // Also, when the anchor is within a table, then do it here as well,
            // as xStart/xEnd would not point to the start/end at conversion
            // time anyway.
            sal_Int32 nTableWidth = 0;
            m_aTableProperties->getValue(TablePropertyMap::TABLE_WIDTH, nTableWidth);
            if (m_rDMapper_Impl.GetSectionContext() && nestedTableLevel <= 1)
                m_rDMapper_Impl.m_aPendingFloatingTables.push_back(FloatingTableInfo(xStart, xEnd, aFrameProperties, nTableWidth));
            else
                m_xText->convertToTextFrame(xStart, xEnd, aFrameProperties);
        }
    }

    m_aTableProperties.reset();
    m_aCellProperties.clear();
    m_aRowProperties.clear();

#ifdef DEBUG_DOMAINMAPPER
    dmapper_logger->endElement();
    dmapper_logger->endElement();
#endif
}

void DomainMapperTableHandler::startRow(unsigned int nCells,
                                        TablePropertyMapPtr pProps)
{
    m_aRowProperties.push_back( pProps );
    m_aCellProperties.push_back( PropertyMapVector1() );

#ifdef DEBUG_DOMAINMAPPER
    dmapper_logger->startElement("table.row");
    dmapper_logger->attribute("cells", nCells);
    if (pProps != NULL)
        pProps->dumpXml(dmapper_logger);
#endif

    m_pRowSeq = RowSequencePointer_t(new RowSequence_t(nCells));
    m_nCellIndex = 0;
}

void DomainMapperTableHandler::endRow()
{
    (*m_pTableSeq)[m_nRowIndex] = *m_pRowSeq;
    ++m_nRowIndex;
    m_nCellIndex = 0;
#ifdef DEBUG_DOMAINMAPPER
    dmapper_logger->endElement();
#endif
}

void DomainMapperTableHandler::startCell(const Handle_t & start,
                                         TablePropertyMapPtr pProps )
{
    sal_uInt32 nRow = m_aRowProperties.size();
    if ( pProps.get( ) )
        m_aCellProperties[nRow - 1].push_back( pProps );
    else
    {
        // Adding an empty cell properties map to be able to get
        // the table defaults properties
        TablePropertyMapPtr pEmptyProps( new TablePropertyMap( ) );
        m_aCellProperties[nRow - 1].push_back( pEmptyProps );
    }

#ifdef DEBUG_DOMAINMAPPER
    dmapper_logger->startElement("table.cell");
    dmapper_logger->startElement("table.cell.start");
    dmapper_logger->chars(toString(start));
    dmapper_logger->endElement();
    if (pProps.get())
        pProps->printProperties();
#endif

    //add a new 'row' of properties
    m_pCellSeq = CellSequencePointer_t(new CellSequence_t(2));
    if (!start.get())
        return;
    (*m_pCellSeq)[0] = start->getStart();
}

void DomainMapperTableHandler::endCell(const Handle_t & end)
{
#ifdef DEBUG_DOMAINMAPPER
    dmapper_logger->startElement("table.cell.end");
    dmapper_logger->chars(toString(end));
    dmapper_logger->endElement();
    dmapper_logger->endElement();
#endif

    if (!end.get())
        return;
    (*m_pCellSeq)[1] = end->getEnd();
    (*m_pRowSeq)[m_nCellIndex] = *m_pCellSeq;
    ++m_nCellIndex;
}

}}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
