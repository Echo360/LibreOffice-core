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

#include "tp_TitleRotation.hxx"

#include "ResId.hxx"
#include "ResourceIds.hrc"
#include "chartview/ChartSfxItemIds.hxx"
#include "HelpIds.hrc"

#include <editeng/eeitem.hxx>
#include <editeng/frmdiritem.hxx>
#include <svl/intitem.hxx>

namespace chart
{

SchAlignmentTabPage::SchAlignmentTabPage(Window* pWindow,
                                         const SfxItemSet& rInAttrs, bool bWithRotation) :
    SfxTabPage(pWindow, "TitleRotationTabPage","modules/schart/ui/titlerotationtabpage.ui", &rInAttrs)
{
    get(m_pCtrlDial,"dialCtrl");
    get(m_pFtRotate,"degreeL");
    get(m_pNfRotate,"OrientDegree");
    get(m_pCbStacked,"stackedCB");
    get(m_pFtTextDirection,"textdirL");
    get(m_pLbTextDirection,"textdirLB");
    get(m_pFtABCD,"labelABCD");
    m_pCtrlDial->SetText(m_pFtABCD->GetText());
    m_pOrientHlp = new svx::OrientationHelper(*m_pCtrlDial, *m_pNfRotate, *m_pCbStacked);

    m_pCbStacked->EnableTriState( false );
    m_pOrientHlp->Enable( true );
    m_pOrientHlp->AddDependentWindow( *m_pFtRotate, TRISTATE_TRUE );

    if( !bWithRotation )
    {
        m_pOrientHlp->Hide();
    }
}

SchAlignmentTabPage::~SchAlignmentTabPage()
{
    delete m_pOrientHlp;
}

SfxTabPage* SchAlignmentTabPage::Create(Window* pParent,
                                        const SfxItemSet* rInAttrs)
{
    return new SchAlignmentTabPage(pParent, *rInAttrs);
}

SfxTabPage* SchAlignmentTabPage::CreateWithoutRotation(Window* pParent,
                                        const SfxItemSet* rInAttrs)
{
    return new SchAlignmentTabPage(pParent, *rInAttrs, false);
}

bool SchAlignmentTabPage::FillItemSet(SfxItemSet* rOutAttrs)
{
    //Since 04/1998 text can be rotated by an arbitrary angle: SCHATTR_TEXT_DEGREES
    bool bStacked = m_pOrientHlp->GetStackedState() == TRISTATE_TRUE;
    rOutAttrs->Put( SfxBoolItem( SCHATTR_TEXT_STACKED, bStacked ) );

    sal_Int32 nDegrees = bStacked ? 0 : m_pCtrlDial->GetRotation();
    rOutAttrs->Put( SfxInt32Item( SCHATTR_TEXT_DEGREES, nDegrees ) );

    SvxFrameDirection aDirection( m_pLbTextDirection->GetSelectEntryValue() );
    rOutAttrs->Put( SfxInt32Item( EE_PARA_WRITINGDIR, aDirection ) );

    return true;
}

void SchAlignmentTabPage::Reset(const SfxItemSet* rInAttrs)
{
    const SfxPoolItem* pItem = GetItem( *rInAttrs, SCHATTR_TEXT_DEGREES );

    sal_Int32 nDegrees = pItem ? ((const SfxInt32Item*)pItem)->GetValue() : 0;
    m_pCtrlDial->SetRotation( nDegrees );

    pItem = GetItem( *rInAttrs, SCHATTR_TEXT_STACKED );
    bool bStacked = pItem && ((const SfxBoolItem*)pItem)->GetValue();
    m_pOrientHlp->SetStackedState( bStacked ? TRISTATE_TRUE : TRISTATE_FALSE );

    if( rInAttrs->GetItemState(EE_PARA_WRITINGDIR, true, &pItem) == SFX_ITEM_SET)
        m_pLbTextDirection->SelectEntryValue( SvxFrameDirection(((const SvxFrameDirectionItem*)pItem)->GetValue()) );
}

} //namespace chart

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
