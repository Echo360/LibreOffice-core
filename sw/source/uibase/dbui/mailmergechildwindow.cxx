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

#include <sfx2/viewfrm.hxx>
#include <sfx2/dispatch.hxx>

#include <cmdid.h>
#include <swmodule.hxx>
#include <view.hxx>
#include <edtwin.hxx>
#include <vcl/layout.hxx>
#include <mailmergechildwindow.hxx>
#include <mmconfigitem.hxx>
#include <svtools/svmedit.hxx>
#include <vcl/msgbox.hxx>
#include <vcl/image.hxx>
#include <mailmergechildwindow.hrc>
#include <dbui.hrc>
#include <helpid.h>

using namespace ::com::sun::star;

SFX_IMPL_FLOATINGWINDOW( SwMailMergeChildWindow, FN_MAILMERGE_CHILDWINDOW )

SwMailMergeChildWindow::SwMailMergeChildWindow( Window* _pParent,
                                sal_uInt16 nId,
                                SfxBindings* pBindings,
                                SfxChildWinInfo* pInfo ) :
                                SfxChildWindow( _pParent, nId )
{
    pWindow = new SwMailMergeChildWin( pBindings, this, _pParent);

    if (!pInfo->aSize.Width() || !pInfo->aSize.Height())
    {
        SwView* pActiveView = ::GetActiveView();
        if(pActiveView)
        {
            const SwEditWin &rEditWin = pActiveView->GetEditWin();
            pWindow->SetPosPixel(rEditWin.OutputToScreenPixel(Point(0, 0)));
        }
        else
            pWindow->SetPosPixel(_pParent->OutputToScreenPixel(Point(0, 0)));
        pInfo->aPos = pWindow->GetPosPixel();
        pInfo->aSize = pWindow->GetSizePixel();
    }

    ((SwMailMergeChildWin *)pWindow)->Initialize(pInfo);
    pWindow->Show();
}

SwMailMergeChildWin::SwMailMergeChildWin( SfxBindings* _pBindings,
                            SfxChildWindow* pChild,
                            Window *pParent) :
    SfxFloatingWindow(_pBindings, pChild, pParent, SW_RES(DLG_MAILMERGECHILD)),
    m_aBackTB(this, SW_RES( TB_BACK ))
{
    m_aBackTB.SetSelectHdl(LINK(this, SwMailMergeChildWin, BackHdl));
    sal_uInt16 nIResId = ILIST_TBX;
    ResId aResId( nIResId, *pSwResMgr );
    ImageList aIList(aResId);
    FreeResource();

    m_aBackTB.SetItemImage( 1, aIList.GetImage(FN_SHOW_ROOT) );
    m_aBackTB.SetButtonType( BUTTON_SYMBOLTEXT );
    Size aSz = m_aBackTB.CalcWindowSizePixel(1);
    m_aBackTB.SetPosSizePixel( Point(), aSz );
    SetOutputSizePixel(aSz);
    m_aBackTB.Show();
}

SwMailMergeChildWin::~SwMailMergeChildWin()
{
}

IMPL_LINK_NOARG(SwMailMergeChildWin, BackHdl)
{
    GetBindings().GetDispatcher()->Execute(FN_MAILMERGE_WIZARD, SFX_CALLMODE_ASYNCHRON);
    return 0;
}

void SwMailMergeChildWin::FillInfo(SfxChildWinInfo& rInfo) const
{
    SfxFloatingWindow::FillInfo(rInfo);
    rInfo.aWinState = OString();
    rInfo.bVisible = false;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
