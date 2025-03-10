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

#include "res_Trendline.hxx"
#include "ResId.hxx"
#include "Strings.hrc"
#include "Bitmaps.hrc"
#include "chartview/ChartSfxItemIds.hxx"

#include <svl/intitem.hxx>
#include <svl/stritem.hxx>
#include <sfx2/tabdlg.hxx>

#include <vector>
#include <algorithm>

namespace chart
{

void lcl_setValue( FormattedField& rFmtField, double fValue )
{
    rFmtField.SetValue( fValue );
    rFmtField.SetDefaultValue( fValue );
}

TrendlineResources::TrendlineResources( Window * pParent, const SfxItemSet& rInAttrs ) :
        m_eTrendLineType( CHREGRESS_LINEAR ),
        m_bTrendLineUnique( true ),
        m_pNumFormatter( NULL ),
        m_nNbPoints( 0 )
{
    SfxTabPage* pTabPage = reinterpret_cast<SfxTabPage*>(pParent);
    pTabPage->get(m_pRB_Linear,"linear");
    pTabPage->get(m_pRB_Logarithmic,"logarithmic");
    pTabPage->get(m_pRB_Exponential,"exponential");
    pTabPage->get(m_pRB_Power,"power");
    pTabPage->get(m_pRB_Polynomial,"polynomial");
    pTabPage->get(m_pRB_MovingAverage,"movingAverage");
    pTabPage->get(m_pNF_Degree,"degree");
    pTabPage->get(m_pNF_Period,"period");
    pTabPage->get(m_pEE_Name,"entry_name");
    pTabPage->get(m_pFmtFld_ExtrapolateForward,"extrapolateForward");
    pTabPage->get(m_pFmtFld_ExtrapolateBackward,"extrapolateBackward");
    pTabPage->get(m_pCB_SetIntercept,"setIntercept");
    pTabPage->get(m_pFmtFld_InterceptValue,"interceptValue");
    pTabPage->get(m_pCB_ShowEquation,"showEquation");
    pTabPage->get(m_pCB_ShowCorrelationCoeff,"showCorrelationCoefficient");
    pTabPage->get(m_pFI_Linear,"imageLinear");
    pTabPage->get(m_pFI_Logarithmic,"imageLogarithmic");
    pTabPage->get(m_pFI_Exponential,"imageExponential");
    pTabPage->get(m_pFI_Power,"imagePower");
    pTabPage->get(m_pFI_Polynomial,"imagePolynomial");
    pTabPage->get(m_pFI_MovingAverage,"imageMovingAverage");
    FillValueSets();

    Link aLink = LINK(this, TrendlineResources, SelectTrendLine );
    m_pRB_Linear->SetClickHdl( aLink );
    m_pRB_Logarithmic->SetClickHdl( aLink );
    m_pRB_Exponential->SetClickHdl( aLink );
    m_pRB_Power->SetClickHdl( aLink );
    m_pRB_Polynomial->SetClickHdl( aLink );
    m_pRB_MovingAverage->SetClickHdl( aLink );

    aLink = LINK(this, TrendlineResources, ChangeValue );
    m_pNF_Degree->SetModifyHdl( aLink );
    m_pNF_Period->SetModifyHdl( aLink );
    m_pFmtFld_InterceptValue->SetModifyHdl( aLink );

    Reset( rInAttrs );
    UpdateControlStates();
}

TrendlineResources::~TrendlineResources()
{}

IMPL_LINK( TrendlineResources, SelectTrendLine, RadioButton *, pRadioButton )
{
    if( pRadioButton == m_pRB_Linear )
        m_eTrendLineType = CHREGRESS_LINEAR;
    else if( pRadioButton == m_pRB_Logarithmic )
        m_eTrendLineType = CHREGRESS_LOG;
    else if( pRadioButton == m_pRB_Exponential )
        m_eTrendLineType = CHREGRESS_EXP;
    else if( pRadioButton == m_pRB_Power )
        m_eTrendLineType = CHREGRESS_POWER;
    else if( pRadioButton == m_pRB_Polynomial )
        m_eTrendLineType = CHREGRESS_POLYNOMIAL;
    else if( pRadioButton == m_pRB_MovingAverage )
        m_eTrendLineType = CHREGRESS_MOVING_AVERAGE;
    m_bTrendLineUnique = true;

    UpdateControlStates();

    return 0;
}

void TrendlineResources::Reset( const SfxItemSet& rInAttrs )
{
    const SfxPoolItem *pPoolItem = NULL;

    if( rInAttrs.GetItemState( SCHATTR_REGRESSION_CURVE_NAME, true, &pPoolItem ) == SFX_ITEM_SET )
    {
        OUString aName = static_cast< const SfxStringItem* >(pPoolItem)->GetValue();
        m_pEE_Name->SetText(aName);
    }
    else
    {
        m_pEE_Name->SetText("");
    }

    SfxItemState aState = rInAttrs.GetItemState( SCHATTR_REGRESSION_TYPE, true, &pPoolItem );
    m_bTrendLineUnique = ( aState != SFX_ITEM_DONTCARE );
    if( aState == SFX_ITEM_SET )
    {
        const SvxChartRegressItem * pItem = dynamic_cast< const SvxChartRegressItem * >( pPoolItem );
        if( pItem )
        {
            m_eTrendLineType = pItem->GetValue();
        }
    }

    if( rInAttrs.GetItemState( SCHATTR_REGRESSION_DEGREE, true, &pPoolItem ) == SFX_ITEM_SET )
    {
        sal_Int32 nDegree = static_cast< const SfxInt32Item * >( pPoolItem )->GetValue();
        m_pNF_Degree->SetValue( nDegree );
    }
    else
    {
        m_pNF_Degree->SetValue( 2 );
    }

    if( rInAttrs.GetItemState( SCHATTR_REGRESSION_PERIOD, true, &pPoolItem ) == SFX_ITEM_SET )
    {
        sal_Int32 nPeriod = static_cast< const SfxInt32Item * >( pPoolItem )->GetValue();
        m_pNF_Period->SetValue( nPeriod );
    }
    else
    {
        m_pNF_Period->SetValue( 2 );
    }

    double nValue = 0.0;
    if( rInAttrs.GetItemState( SCHATTR_REGRESSION_EXTRAPOLATE_FORWARD, true, &pPoolItem ) == SFX_ITEM_SET )
    {
        nValue = ((const SvxDoubleItem*)pPoolItem)->GetValue() ;
    }
    lcl_setValue( *m_pFmtFld_ExtrapolateForward, nValue );

    nValue = 0.0;
    if( rInAttrs.GetItemState( SCHATTR_REGRESSION_EXTRAPOLATE_BACKWARD, true, &pPoolItem ) == SFX_ITEM_SET )
    {
        nValue = ((const SvxDoubleItem*)pPoolItem)->GetValue() ;
    }
    lcl_setValue( *m_pFmtFld_ExtrapolateBackward, nValue );

    nValue = 0.0;
    if( rInAttrs.GetItemState( SCHATTR_REGRESSION_INTERCEPT_VALUE, true, &pPoolItem ) == SFX_ITEM_SET )
    {
        nValue = ((const SvxDoubleItem*)pPoolItem)->GetValue() ;
    }
    lcl_setValue( *m_pFmtFld_InterceptValue, nValue );

    aState = rInAttrs.GetItemState( SCHATTR_REGRESSION_SET_INTERCEPT, true, &pPoolItem );
    if( aState == SFX_ITEM_DONTCARE )
    {
        m_pCB_SetIntercept->EnableTriState( true );
        m_pCB_SetIntercept->SetState( TRISTATE_INDET );
    }
    else
    {
        m_pCB_SetIntercept->EnableTriState( false );
        if( aState == SFX_ITEM_SET )
            m_pCB_SetIntercept->Check( static_cast< const SfxBoolItem * >( pPoolItem )->GetValue());
    }

    aState = rInAttrs.GetItemState( SCHATTR_REGRESSION_SHOW_EQUATION, true, &pPoolItem );
    if( aState == SFX_ITEM_DONTCARE )
    {
        m_pCB_ShowEquation->EnableTriState( true );
        m_pCB_ShowEquation->SetState( TRISTATE_INDET );
    }
    else
    {
        m_pCB_ShowEquation->EnableTriState( false );
        if( aState == SFX_ITEM_SET )
            m_pCB_ShowEquation->Check( static_cast< const SfxBoolItem * >( pPoolItem )->GetValue());
    }

    aState = rInAttrs.GetItemState( SCHATTR_REGRESSION_SHOW_COEFF, true, &pPoolItem );
    if( aState == SFX_ITEM_DONTCARE )
    {
        m_pCB_ShowCorrelationCoeff->EnableTriState( true );
        m_pCB_ShowCorrelationCoeff->SetState( TRISTATE_INDET );
    }
    else
    {
        m_pCB_ShowCorrelationCoeff->EnableTriState( false );
        if( aState == SFX_ITEM_SET )
            m_pCB_ShowCorrelationCoeff->Check( static_cast< const SfxBoolItem * >( pPoolItem )->GetValue());
    }

    if( m_bTrendLineUnique )
    {
        switch( m_eTrendLineType )
        {
            case CHREGRESS_LINEAR :
                m_pRB_Linear->Check();
                break;
            case CHREGRESS_LOG :
                m_pRB_Logarithmic->Check();
                break;
            case CHREGRESS_EXP :
                m_pRB_Exponential->Check();
                break;
            case CHREGRESS_POWER :
                m_pRB_Power->Check();
                break;
            case CHREGRESS_POLYNOMIAL :
                m_pRB_Polynomial->Check();
                break;
            case CHREGRESS_MOVING_AVERAGE :
                m_pRB_MovingAverage->Check();
                break;
            default:
                break;
        }
    }
}

bool TrendlineResources::FillItemSet(SfxItemSet* rOutAttrs) const
{
    if( m_bTrendLineUnique )
        rOutAttrs->Put( SvxChartRegressItem( m_eTrendLineType, SCHATTR_REGRESSION_TYPE ));

    if( m_pCB_ShowEquation->GetState() != TRISTATE_INDET )
        rOutAttrs->Put( SfxBoolItem( SCHATTR_REGRESSION_SHOW_EQUATION, m_pCB_ShowEquation->IsChecked() ));

    if( m_pCB_ShowCorrelationCoeff->GetState() != TRISTATE_INDET )
        rOutAttrs->Put( SfxBoolItem( SCHATTR_REGRESSION_SHOW_COEFF, m_pCB_ShowCorrelationCoeff->IsChecked() ));

    OUString aName = m_pEE_Name->GetText();
    rOutAttrs->Put(SfxStringItem(SCHATTR_REGRESSION_CURVE_NAME, aName));

    sal_Int32 aDegree = m_pNF_Degree->GetValue();
    rOutAttrs->Put(SfxInt32Item( SCHATTR_REGRESSION_DEGREE, aDegree ) );

    sal_Int32 aPeriod = m_pNF_Period->GetValue();
    rOutAttrs->Put(SfxInt32Item( SCHATTR_REGRESSION_PERIOD, aPeriod ) );

    sal_uInt32 nIndex = 0;
    double aValue = 0.0;
    m_pNumFormatter->IsNumberFormat(m_pFmtFld_ExtrapolateForward->GetText(),nIndex,aValue);
    rOutAttrs->Put(SvxDoubleItem( aValue, SCHATTR_REGRESSION_EXTRAPOLATE_FORWARD ) );

    aValue = 0.0;
    m_pNumFormatter->IsNumberFormat(m_pFmtFld_ExtrapolateBackward->GetText(),nIndex,aValue);
    rOutAttrs->Put(SvxDoubleItem( aValue, SCHATTR_REGRESSION_EXTRAPOLATE_BACKWARD ) );

    if( m_pCB_SetIntercept->GetState() != TRISTATE_INDET )
        rOutAttrs->Put( SfxBoolItem( SCHATTR_REGRESSION_SET_INTERCEPT, m_pCB_SetIntercept->IsChecked() ));

    aValue = 0.0;
    m_pNumFormatter->IsNumberFormat(m_pFmtFld_InterceptValue->GetText(),nIndex,aValue);
    rOutAttrs->Put(SvxDoubleItem( aValue, SCHATTR_REGRESSION_INTERCEPT_VALUE ) );

    return true;
}

void TrendlineResources::FillValueSets()
{
    m_pFI_Linear->SetImage(       Image( SchResId( BMP_REGRESSION_LINEAR          ) ) );
    m_pFI_Logarithmic->SetImage(  Image( SchResId( BMP_REGRESSION_LOG             ) ) );
    m_pFI_Exponential->SetImage(  Image( SchResId( BMP_REGRESSION_EXP             ) ) );
    m_pFI_Power->SetImage(        Image( SchResId( BMP_REGRESSION_POWER           ) ) );
    m_pFI_Polynomial->SetImage(   Image( SchResId( BMP_REGRESSION_POLYNOMIAL      ) ) );
    m_pFI_MovingAverage->SetImage(Image( SchResId( BMP_REGRESSION_MOVING_AVERAGE  ) ) );
}

void TrendlineResources::UpdateControlStates()
{
    if( m_nNbPoints > 0 )
    {
        sal_Int32 nMaxValue = m_nNbPoints - 1 + ( m_pCB_SetIntercept->IsChecked()?1:0 );
//        if( nMaxValue > 10) nMaxValue = 10;
        m_pNF_Degree->SetMax( nMaxValue );
        m_pNF_Period->SetMax( m_nNbPoints - 1 );
    }
    bool bMovingAverage = ( m_eTrendLineType == CHREGRESS_MOVING_AVERAGE );
    bool bInterceptAvailable = ( m_eTrendLineType == CHREGRESS_LINEAR ) || ( m_eTrendLineType == CHREGRESS_POLYNOMIAL );
    m_pFmtFld_ExtrapolateForward->Enable( !bMovingAverage );
    m_pFmtFld_ExtrapolateBackward->Enable( !bMovingAverage );
    m_pCB_SetIntercept->Enable( bInterceptAvailable );
    m_pFmtFld_InterceptValue->Enable( bInterceptAvailable );
    if( bMovingAverage )
    {
        m_pCB_ShowEquation->SetState( TRISTATE_FALSE );
        m_pCB_ShowCorrelationCoeff->SetState( TRISTATE_FALSE );
    }
    m_pCB_ShowEquation->Enable( !bMovingAverage );
    m_pCB_ShowCorrelationCoeff->Enable( !bMovingAverage );
}

IMPL_LINK( TrendlineResources, ChangeValue, void *, pNumericField)
{
    if( pNumericField == m_pNF_Degree )
    {
        if( !m_pRB_Polynomial->IsChecked() )
        {
                m_pRB_Polynomial->Check();
                SelectTrendLine(m_pRB_Polynomial);
        }
    }
    else if( pNumericField == m_pNF_Period )
    {
        if( !m_pRB_MovingAverage->IsChecked() )
        {
                m_pRB_MovingAverage->Check();
                SelectTrendLine(m_pRB_MovingAverage);
        }
    }
    else if( pNumericField == m_pFmtFld_InterceptValue )
    {
        if( !m_pCB_SetIntercept->IsChecked() )
                m_pCB_SetIntercept->Check();
    }
    UpdateControlStates();

    return 0;
}

void TrendlineResources::SetNumFormatter( SvNumberFormatter* pFormatter )
{
    m_pNumFormatter = pFormatter;
    m_pFmtFld_ExtrapolateForward->SetFormatter( m_pNumFormatter );
    m_pFmtFld_ExtrapolateBackward->SetFormatter( m_pNumFormatter );
    m_pFmtFld_InterceptValue->SetFormatter( m_pNumFormatter );
}

void TrendlineResources::SetNbPoints( sal_Int32 nNbPoints )
{
    m_nNbPoints = nNbPoints;
    UpdateControlStates();
}

} //  namespace chart

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
