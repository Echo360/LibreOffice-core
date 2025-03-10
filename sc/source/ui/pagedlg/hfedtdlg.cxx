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

#undef SC_DLLIMPLEMENTATION

#include "scitems.hxx"
#include <svl/eitem.hxx>

#include "hfedtdlg.hxx"
#include "global.hxx"
#include "globstr.hrc"
#include "scresid.hxx"
#include "scuitphfedit.hxx"

//  macros from docsh4.cxx
//! use SIDs?

#define IS_SHARE_HEADER(set) \
    ((SfxBoolItem&) \
        ((SvxSetItem&)(set).Get(ATTR_PAGE_HEADERSET)).GetItemSet(). \
            Get(ATTR_PAGE_SHARED)).GetValue()

#define IS_SHARE_FOOTER(set) \
    ((SfxBoolItem&) \
        ((SvxSetItem&)(set).Get(ATTR_PAGE_FOOTERSET)).GetItemSet(). \
            Get(ATTR_PAGE_SHARED)).GetValue()

ScHFEditDlg::ScHFEditDlg( SfxViewFrame*     pFrameP,
                          Window*           pParent,
                          const SfxItemSet& rCoreSet,
                          const OUString&   rPageStyle,
                          const OString& rID, const OUString& rUIXMLDescription )
    :   SfxTabDialog( pFrameP, pParent, rID, rUIXMLDescription, &rCoreSet )
{
    eNumType = ((const SvxPageItem&)rCoreSet.Get(ATTR_PAGE)).GetNumType();

    OUString aTmp = GetText();

    aTmp += " (" + ScGlobal::GetRscString( STR_PAGESTYLE ) + ": " + rPageStyle + ")";

    SetText( aTmp );
}

ScHFEditHeaderDlg::ScHFEditHeaderDlg( SfxViewFrame*     pFrameP,
                          Window*           pParent,
                          const SfxItemSet& rCoreSet,
                          const OUString&   rPageStyle)
    :   ScHFEditDlg( pFrameP, pParent, rCoreSet, rPageStyle,
        "HeaderDialog", "modules/scalc/ui/headerdialog.ui" )
{
    AddTabPage( "headerright", ScRightHeaderEditPage::Create, NULL );
    AddTabPage( "headerleft", ScLeftHeaderEditPage::Create, NULL );
}

ScHFEditFooterDlg::ScHFEditFooterDlg( SfxViewFrame*     pFrameP,
                          Window*           pParent,
                          const SfxItemSet& rCoreSet,
                          const OUString&   rPageStyle)
    :   ScHFEditDlg( pFrameP, pParent, rCoreSet, rPageStyle,
        "FooterDialog", "modules/scalc/ui/footerdialog.ui" )
{
    AddTabPage( "footerright", ScRightFooterEditPage::Create, NULL );
    AddTabPage( "footerleft", ScLeftFooterEditPage::Create, NULL );
}

ScHFEditLeftHeaderDlg::ScHFEditLeftHeaderDlg( SfxViewFrame*     pFrameP,
                          Window*           pParent,
                          const SfxItemSet& rCoreSet,
                          const OUString&   rPageStyle)
    :   ScHFEditDlg( pFrameP, pParent, rCoreSet, rPageStyle,
        "LeftHeaderDialog", "modules/scalc/ui/leftheaderdialog.ui" )
{
    AddTabPage( "headerleft", ScLeftHeaderEditPage::Create, NULL );
}

ScHFEditRightHeaderDlg::ScHFEditRightHeaderDlg( SfxViewFrame*     pFrameP,
                          Window*           pParent,
                          const SfxItemSet& rCoreSet,
                          const OUString&   rPageStyle)
    :   ScHFEditDlg( pFrameP, pParent, rCoreSet, rPageStyle,
        "RightHeaderDialog", "modules/scalc/ui/rightheaderdialog.ui" )
{
    AddTabPage( "headerright", ScRightHeaderEditPage::Create, NULL );
}

ScHFEditLeftFooterDlg::ScHFEditLeftFooterDlg( SfxViewFrame*     pFrameP,
                          Window*           pParent,
                          const SfxItemSet& rCoreSet,
                          const OUString&   rPageStyle)
    :   ScHFEditDlg( pFrameP, pParent, rCoreSet, rPageStyle,
        "LeftFooterDialog", "modules/scalc/ui/leftfooterdialog.ui" )
{
    AddTabPage( "footerleft", ScLeftFooterEditPage::Create, NULL );
}

ScHFEditRightFooterDlg::ScHFEditRightFooterDlg( SfxViewFrame*     pFrameP,
                          Window*           pParent,
                          const SfxItemSet& rCoreSet,
                          const OUString&   rPageStyle)
    :   ScHFEditDlg( pFrameP, pParent, rCoreSet, rPageStyle,
        "RightFooterDialog", "modules/scalc/ui/rightfooterdialog.ui" )
{
    AddTabPage( "footerright", ScRightFooterEditPage::Create, NULL );
}

ScHFEditSharedHeaderDlg::ScHFEditSharedHeaderDlg( SfxViewFrame*     pFrameP,
                          Window*           pParent,
                          const SfxItemSet& rCoreSet,
                          const OUString&   rPageStyle)
    :   ScHFEditDlg( pFrameP, pParent, rCoreSet, rPageStyle,
        "SharedHeaderDialog", "modules/scalc/ui/sharedheaderdialog.ui" )
{
    AddTabPage( "header", ScRightHeaderEditPage::Create, NULL );
    AddTabPage( "footerright", ScRightFooterEditPage::Create, NULL );
    AddTabPage( "footerleft", ScLeftFooterEditPage::Create,  NULL );
}

ScHFEditSharedFooterDlg::ScHFEditSharedFooterDlg( SfxViewFrame*     pFrameP,
                          Window*           pParent,
                          const SfxItemSet& rCoreSet,
                          const OUString&   rPageStyle)
    :   ScHFEditDlg( pFrameP, pParent, rCoreSet, rPageStyle,
        "SharedFooterDialog", "modules/scalc/ui/sharedfooterdialog.ui" )
{
    AddTabPage( "headerright", ScRightHeaderEditPage::Create, NULL );
    AddTabPage( "headerleft", ScLeftHeaderEditPage::Create, NULL );
    AddTabPage( "footer", ScRightFooterEditPage::Create, NULL );
}

ScHFEditAllDlg::ScHFEditAllDlg( SfxViewFrame*     pFrameP,
                          Window*           pParent,
                          const SfxItemSet& rCoreSet,
                          const OUString&   rPageStyle)
    :   ScHFEditDlg( pFrameP, pParent, rCoreSet, rPageStyle,
        "AllHeaderFooterDialog", "modules/scalc/ui/allheaderfooterdialog.ui" )
{
    AddTabPage( "headerright", ScRightHeaderEditPage::Create, NULL );
    AddTabPage( "headerleft", ScLeftHeaderEditPage::Create, NULL );
    AddTabPage( "footerright", ScRightFooterEditPage::Create, NULL );
    AddTabPage( "footerleft", ScLeftFooterEditPage::Create, NULL );
}

ScHFEditActiveDlg::ScHFEditActiveDlg( SfxViewFrame*     pFrameP,
                          Window*           pParent,
                          const SfxItemSet& rCoreSet,
                          const OUString&   rPageStyle)
    :   ScHFEditDlg( pFrameP, pParent, rCoreSet, rPageStyle,
        "HeaderFooterDialog", "modules/scalc/ui/headerfooterdialog.ui" )
{
    const SvxPageItem&  rPageItem = (const SvxPageItem&)
                rCoreSet.Get(
                    rCoreSet.GetPool()->GetWhich(SID_ATTR_PAGE) );

    bool bRightPage = ( SVX_PAGE_LEFT !=
                        SvxPageUsage(rPageItem.GetPageUsage()) );

    if ( bRightPage )
    {
        AddTabPage( "header", ScRightHeaderEditPage::Create, NULL );
        AddTabPage( "footer", ScRightFooterEditPage::Create, NULL );
    }
    else
    {
        //  #69193a# respect "shared" setting

        bool bShareHeader = IS_SHARE_HEADER(rCoreSet);
        if ( bShareHeader )
            AddTabPage( "header", ScRightHeaderEditPage::Create, NULL );
        else
            AddTabPage( "header", ScLeftHeaderEditPage::Create, NULL );

        bool bShareFooter = IS_SHARE_FOOTER(rCoreSet);
        if ( bShareFooter )
            AddTabPage( "footer", ScRightFooterEditPage::Create, NULL );
        else
            AddTabPage( "footer", ScLeftFooterEditPage::Create, NULL );
    }
}

void ScHFEditDlg::PageCreated( sal_uInt16 /* nId */, SfxTabPage& rPage )
{
    // kann ja nur ne ScHFEditPage sein...

    ((ScHFEditPage&)rPage).SetNumType(eNumType);
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
