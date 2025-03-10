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

#include <sfx2/app.hxx>
#include <tools/rc.h>
#include <tools/shl.hxx>

#include <dialmgr.hxx>

#include "iconcdlg.hxx"

#include "helpid.hrc"
#include <cuires.hrc>
#include <unotools/viewoptions.hxx>
#include <svtools/apearcfg.hxx>
#include <vcl/mnemonic.hxx>
#include <vcl/i18nhelp.hxx>
#include <vcl/settings.hxx>

using ::std::vector;

/**********************************************************************
|
| Ctor / Dtor
|
\**********************************************************************/

IconChoicePage::IconChoicePage( Window *pParent, const OString& rID,
                                const OUString& rUIXMLDescription,
                                const SfxItemSet &rAttrSet )
:   TabPage                   ( pParent, rID, rUIXMLDescription ),
    pSet                      ( &rAttrSet ),
    bHasExchangeSupport       ( false ),
    pDialog                   ( NULL )
{
    SetStyle ( GetStyle()  | WB_DIALOGCONTROL | WB_HIDE );
}



IconChoicePage::~IconChoicePage()
{
}

/**********************************************************************
|
| Activate / Deaktivate
|
\**********************************************************************/

void IconChoicePage::ActivatePage( const SfxItemSet& )
{
}



int IconChoicePage::DeactivatePage( SfxItemSet* )
{
    return LEAVE_PAGE;
}

/**********************************************************************
|
| ...
|
\**********************************************************************/

void IconChoicePage::FillUserData()
{
}



bool IconChoicePage::IsReadOnly() const
{
    return false;
}



bool IconChoicePage::QueryClose()
{
    return true;
}

/**********************************************************************
|
| window-methods
|
\**********************************************************************/

void IconChoicePage::ImplInitSettings()
{
    Window* pParent = GetParent();
    if ( pParent->IsChildTransparentModeEnabled() && !IsControlBackground() )
    {
        EnableChildTransparentMode( true );
        SetParentClipMode( PARENTCLIPMODE_NOCLIP );
        SetPaintTransparent( true );
        SetBackground();
    }
    else
    {
        EnableChildTransparentMode( false );
        SetParentClipMode( 0 );
        SetPaintTransparent( false );

        if ( IsControlBackground() )
            SetBackground( GetControlBackground() );
        else
            SetBackground( pParent->GetBackground() );
    }
}



void IconChoicePage::StateChanged( StateChangedType nType )
{
    Window::StateChanged( nType );

    if ( nType == STATE_CHANGE_CONTROLBACKGROUND )
    {
        ImplInitSettings();
        Invalidate();
    }
}



void IconChoicePage::DataChanged( const DataChangedEvent& rDCEvt )
{
    Window::DataChanged( rDCEvt );

    if ( (rDCEvt.GetType() == DATACHANGED_SETTINGS) &&
         (rDCEvt.GetFlags() & SETTINGS_STYLE) )
    {
        ImplInitSettings();
        Invalidate();
    }
}



// Class IconChoiceDialog



/**********************************************************************
|
| Ctor / Dtor
|
\**********************************************************************/

extern "C" SAL_DLLPUBLIC_EXPORT Window* SAL_CALL makeSvtIconChoiceCtrl(Window *pParent, VclBuilder::stringmap &)
{
    return new SvtIconChoiceCtrl(pParent, WB_3DLOOK | WB_ICON | WB_BORDER |
                            WB_NOCOLUMNHEADER | WB_HIGHLIGHTFRAME |
                            WB_NODRAGSELECTION | WB_TABSTOP);
}

IconChoiceDialog::IconChoiceDialog ( Window* pParent, const OString& rID,
                                     const OUString& rUIXMLDescription,
                                     const SfxItemSet *pItemSet )
:   ModalDialog         ( pParent, rID, rUIXMLDescription ),
    mnCurrentPageId ( USHRT_MAX ),

    pSet            ( pItemSet ),
    pOutSet         ( NULL ),
    pExampleSet     ( NULL ),
    pRanges         ( NULL ),

    bHideResetBtn   ( false ),
    bModal          ( false ),
    bInOK           ( false ),
    bItemsReset     ( false )
{
    get(m_pOKBtn, "ok");
    get(m_pCancelBtn, "cancel");
    get(m_pHelpBtn, "help");
    get(m_pResetBtn, "back");
    get(m_pIconCtrl, "icon_control");
    get(m_pTabContainer, "tab");

    SetCtrlStyle();
    m_pIconCtrl->SetClickHdl ( LINK ( this, IconChoiceDialog , ChosePageHdl_Impl ) );
    m_pIconCtrl->Show();
    m_pIconCtrl->SetChoiceWithCursor ( true );
    m_pIconCtrl->SetSelectionMode( SINGLE_SELECTION );
    m_pIconCtrl->SetHelpId( HID_ICCDIALOG_CHOICECTRL );

    // ItemSet
    if ( pSet )
    {
        pExampleSet = new SfxItemSet( *pSet );
        pOutSet = new SfxItemSet( *pSet->GetPool(), pSet->GetRanges() );
    }

    // Buttons
    m_pOKBtn->SetClickHdl   ( LINK( this, IconChoiceDialog, OkHdl ) );
    m_pOKBtn->SetHelpId( HID_ICCDIALOG_OK_BTN );
    m_pCancelBtn->SetHelpId( HID_ICCDIALOG_CANCEL_BTN );
    m_pResetBtn->SetClickHdl( LINK( this, IconChoiceDialog, ResetHdl ) );
    m_pResetBtn->SetText( CUI_RESSTR(RID_SVXSTR_ICONCHOICEDLG_RESETBUT) );
    m_pResetBtn->SetHelpId( HID_ICCDIALOG_RESET_BTN );
    m_pOKBtn->Show();
    m_pCancelBtn->Show();
    m_pHelpBtn->Show();
    m_pResetBtn->Show();
}

IconChoiceDialog ::~IconChoiceDialog ()
{
    // save configuration at INI-Manager
    // and remove pages
    //SvtViewOptions aTabDlgOpt( E_TABDIALOG, rId );
    //aTabDlgOpt.SetWindowState(OStringToOUString(GetWindowState((WINDOWSTATE_MASK_X | WINDOWSTATE_MASK_Y | WINDOWSTATE_MASK_STATE | WINDOWSTATE_MASK_MINIMIZED)), RTL_TEXTENCODING_ASCII_US));
    //aTabDlgOpt.SetPageID( mnCurrentPageId );

    for ( size_t i = 0, nCount = maPageList.size(); i < nCount; ++i )
    {
        IconChoicePageData* pData = maPageList[ i ];

        if ( pData->pPage )
        {
            pData->pPage->FillUserData();
            OUString aPageData(pData->pPage->GetUserData());
            if ( !aPageData.isEmpty() )
            {
                //SvtViewOptions aTabPageOpt( E_TABPAGE, OUString::number(pData->nId) );

                //SetViewOptUserItem( aTabPageOpt, aPageData );
            }

            if ( pData->bOnDemand )
                delete (SfxItemSet*)&pData->pPage->GetItemSet();
            delete pData->pPage;
        }
        delete pData;
    }

    // remove Userdata from Icons
    for ( sal_uLong i=0; i < m_pIconCtrl->GetEntryCount(); i++)
    {
        SvxIconChoiceCtrlEntry* pEntry = m_pIconCtrl->GetEntry ( i );
        sal_uInt16* pUserData = (sal_uInt16*) pEntry->GetUserData();
        delete pUserData;
    }


    delete pRanges;
    delete pOutSet;
}

/**********************************************************************
|
| add new page
|
\**********************************************************************/

SvxIconChoiceCtrlEntry* IconChoiceDialog::AddTabPage(
    sal_uInt16          nId,
    const OUString&   rIconText,
    const Image&    rChoiceIcon,
    CreatePage      pCreateFunc /* != 0 */,
    GetPageRanges   pRangesFunc /* darf 0 sein */,
    bool            bItemsOnDemand,
    sal_uLong           /*nPos*/
)
{
    IconChoicePageData* pData = new IconChoicePageData ( nId, pCreateFunc,
                                                         pRangesFunc,
                                                         bItemsOnDemand );
    maPageList.push_back( pData );

    pData->fnGetRanges = pRangesFunc;
    pData->bOnDemand = bItemsOnDemand;

    sal_uInt16 *pId = new sal_uInt16 ( nId );
    SvxIconChoiceCtrlEntry* pEntry = m_pIconCtrl->InsertEntry( rIconText, rChoiceIcon );
    pEntry->SetUserData ( (void*) pId );
    return pEntry;
}

void IconChoiceDialog::SetCtrlStyle()
{
    WinBits aWinBits = WB_3DLOOK | WB_ICON | WB_BORDER | WB_NOCOLUMNHEADER | WB_HIGHLIGHTFRAME | WB_NODRAGSELECTION | WB_TABSTOP | WB_CLIPCHILDREN | WB_ALIGN_LEFT | WB_NOHSCROLL;
    m_pIconCtrl->SetStyle(aWinBits);
    m_pIconCtrl->ArrangeIcons();
}

/**********************************************************************
|
| Show / Hide page or button
|
\**********************************************************************/

void IconChoiceDialog::ShowPageImpl ( IconChoicePageData* pData )
{
    if ( pData->pPage )
        pData->pPage->Show();
}



void IconChoiceDialog::HidePageImpl ( IconChoicePageData* pData )
{
    if ( pData->pPage )
        pData->pPage->Hide();
}

void IconChoiceDialog::ShowPage(sal_uInt16 nId)
{
    sal_uInt16 nOldPageId = GetCurPageId();
    bool bInvalidate = nOldPageId != nId;
    SetCurPageId(nId);
    ActivatePageImpl();
    if (bInvalidate)
    {
        IconChoicePageData* pOldData = GetPageData(nOldPageId);
        if (pOldData && pOldData->pPage)
        {
            DeActivatePageImpl();
            HidePageImpl(pOldData);
        }

        Invalidate();
    }
    IconChoicePageData* pNewData = GetPageData(nId);
    if (pNewData && pNewData->pPage)
        ShowPageImpl(pNewData);
}

/**********************************************************************
|
| select a page
|
\**********************************************************************/
IMPL_LINK_NOARG(IconChoiceDialog , ChosePageHdl_Impl)
{
    sal_uLong nPos;

    SvxIconChoiceCtrlEntry *pEntry = m_pIconCtrl->GetSelectedEntry ( nPos );
    if ( !pEntry )
        pEntry = m_pIconCtrl->GetCursor( );

    sal_uInt16 *pId = (sal_uInt16*)pEntry->GetUserData ();

    if( *pId != mnCurrentPageId )
    {
        ShowPage(*pId);
    }

    return 0L;
}

/**********************************************************************
|
| Button-handler
|
\**********************************************************************/

IMPL_LINK_NOARG(IconChoiceDialog, OkHdl)
{
    bInOK = true;

    if ( OK_Impl() )
    {
        if ( bModal )
            EndDialog( Ok() );
        else
        {
            Ok();
            Close();
        }
    }
    return 0;
}



IMPL_LINK_NOARG(IconChoiceDialog, ResetHdl)
{
    ResetPageImpl ();

    IconChoicePageData* pData = GetPageData ( mnCurrentPageId );
    DBG_ASSERT( pData, "Id nicht bekannt" );

    if ( pData->bOnDemand )
    {
        // CSet on AIS has problems here, therefore separated
        const SfxItemSet* _pSet = &( pData->pPage->GetItemSet() );
        pData->pPage->Reset( *(SfxItemSet*)_pSet );
    }
    else
        pData->pPage->Reset( *pSet );


    return 0;
}



IMPL_LINK_NOARG(IconChoiceDialog, CancelHdl)
{
    Close();

    return 0;
}

/**********************************************************************
|
| call page
|
\**********************************************************************/

void IconChoiceDialog::ActivatePageImpl ()
{
    DBG_ASSERT( !maPageList.empty(), "keine Pages angemeldet" );
    IconChoicePageData* pData = GetPageData ( mnCurrentPageId );
    DBG_ASSERT( pData, "Id nicht bekannt" );
    bool bReadOnly = false;
    if ( pData )
    {
        if ( !pData->pPage )
        {
            const SfxItemSet* pTmpSet = 0;

            if ( pSet )
            {
                if ( bItemsReset && pSet->GetParent() )
                    pTmpSet = pSet->GetParent();
                else
                    pTmpSet = pSet;
            }

            if ( pTmpSet && !pData->bOnDemand )
                pData->pPage = (pData->fnCreatePage)( m_pTabContainer, this, *pTmpSet );
            else
                pData->pPage = (pData->fnCreatePage)( m_pTabContainer, this, *CreateInputItemSet( mnCurrentPageId ) );

            if ( pData->bOnDemand )
                pData->pPage->Reset( (SfxItemSet &)pData->pPage->GetItemSet() );
            else
                pData->pPage->Reset( *pSet );

            PageCreated( mnCurrentPageId, *pData->pPage );
        }
        else if ( pData->bRefresh )
        {
            pData->pPage->Reset( *pSet );
        }

        pData->bRefresh = false;

        if ( pExampleSet )
            pData->pPage->ActivatePage( *pExampleSet );
        SetHelpId( pData->pPage->GetHelpId() );
        bReadOnly = pData->pPage->IsReadOnly();
    }


    if ( bReadOnly || bHideResetBtn )
        m_pResetBtn->Hide();
    else
        m_pResetBtn->Show();

}



bool IconChoiceDialog::DeActivatePageImpl ()
{
    IconChoicePageData *pData = GetPageData ( mnCurrentPageId );

    int nRet = IconChoicePage::LEAVE_PAGE;

    if ( pData )
    {
        IconChoicePage * pPage = pData->pPage;

        if ( !pExampleSet && pPage->HasExchangeSupport() && pSet )
            pExampleSet = new SfxItemSet( *pSet->GetPool(), pSet->GetRanges() );

        if ( pSet )
        {
            SfxItemSet aTmp( *pSet->GetPool(), pSet->GetRanges() );

            if ( pPage->HasExchangeSupport() )
                nRet = pPage->DeactivatePage( &aTmp );

            if ( ( IconChoicePage::LEAVE_PAGE & nRet ) == IconChoicePage::LEAVE_PAGE &&
                 aTmp.Count() )
            {
                pExampleSet->Put( aTmp );
                pOutSet->Put( aTmp );
            }
        }
        else
        {
            if ( pPage->HasExchangeSupport() ) //!!!
            {
                if ( !pExampleSet )
                {
                    SfxItemPool* pPool = pPage->GetItemSet().GetPool();
                    pExampleSet =
                        new SfxItemSet( *pPool, GetInputRanges( *pPool ) );
                }
                nRet = pPage->DeactivatePage( pExampleSet );
            }
            else
                nRet = pPage->DeactivatePage( NULL );
        }

        if ( nRet & IconChoicePage::REFRESH_SET )
        {
            pSet = GetRefreshedSet();
            DBG_ASSERT( pSet, "GetRefreshedSet() liefert NULL" );
            // flag all pages to be newly initialized
            for ( size_t i = 0, nCount = maPageList.size(); i < nCount; ++i )
            {
                IconChoicePageData* pObj = maPageList[ i ];
                if ( pObj->pPage != pPage )
                    pObj->bRefresh = true;
                else
                    pObj->bRefresh = false;
            }
        }
    }

    if ( nRet & IconChoicePage::LEAVE_PAGE )
        return true;
    else
        return false;
}



void IconChoiceDialog::ResetPageImpl ()
{
    IconChoicePageData *pData = GetPageData ( mnCurrentPageId );

    DBG_ASSERT( pData, "Id nicht bekannt" );

    if ( pData->bOnDemand )
    {
        // CSet on AIS has problems here, therefore separated
        const SfxItemSet* _pSet = &pData->pPage->GetItemSet();
        pData->pPage->Reset( *(SfxItemSet*)_pSet );
    }
    else
        pData->pPage->Reset( *pSet );
}

/**********************************************************************
|
| handling itemsets
|
\**********************************************************************/

const sal_uInt16* IconChoiceDialog::GetInputRanges( const SfxItemPool& rPool )
{
    if ( pSet )
    {
        SAL_WARN( "cui.dialogs", "Set does already exist!" );
        return pSet->GetRanges();
    }

    if ( pRanges )
        return pRanges;
    std::vector<sal_uInt16> aUS;

    size_t nCount = maPageList.size();
    for ( size_t i = 0; i < nCount; ++i )
    {
        IconChoicePageData* pData = maPageList[ i ];
        if ( pData->fnGetRanges )
        {
            const sal_uInt16* pTmpRanges = (pData->fnGetRanges)();
            const sal_uInt16* pIter = pTmpRanges;

            sal_uInt16 nLen;
            for( nLen = 0; *pIter; ++nLen, ++pIter )
                ;
            aUS.insert( aUS.end(), pTmpRanges, pTmpRanges + nLen );
        }
    }

    // remove double Id's
    {
        nCount = aUS.size();
        for ( size_t i = 0; i < nCount; ++i )
            aUS[i] = rPool.GetWhich( aUS[i] );
    }

    if ( aUS.size() > 1 )
    {
        std::sort( aUS.begin(), aUS.end() );
    }

    pRanges = new sal_uInt16[aUS.size() + 1];
    std::copy( aUS.begin(), aUS.end(), pRanges );
    pRanges[aUS.size()] = 0;

    return pRanges;
}



void IconChoiceDialog::SetInputSet( const SfxItemSet* pInSet )
{
    bool bSet = ( pSet != NULL );

    pSet = pInSet;

    if ( !bSet && !pExampleSet && !pOutSet )
    {
        pExampleSet = new SfxItemSet( *pSet );
        pOutSet = new SfxItemSet( *pSet->GetPool(), pSet->GetRanges() );
    }
}



void IconChoiceDialog::PageCreated( sal_uInt16 /*nId*/, IconChoicePage& /*rPage*/ )
{
    // not interested in
}



SfxItemSet* IconChoiceDialog::CreateInputItemSet( sal_uInt16 )
{
    SAL_INFO( "cui.dialogs", "CreateInputItemSet not implemented" );

    return 0;
}

/**********************************************************************
|
| start dialog
|
\**********************************************************************/

short IconChoiceDialog::Execute()
{
    if ( maPageList.empty() )
        return RET_CANCEL;

    Start_Impl();

    return Dialog::Execute();
}



void IconChoiceDialog::Start( bool bShow )
{

    m_pCancelBtn->SetClickHdl( LINK( this, IconChoiceDialog, CancelHdl ) );
    bModal = false;

    Start_Impl();

    if ( bShow )
        Window::Show();

}



bool IconChoiceDialog::QueryClose()
{
    bool bRet = true;
    for ( size_t i = 0, nCount = maPageList.size(); i < nCount; ++i )
    {
        IconChoicePageData* pData = maPageList[i ];
        if ( pData->pPage && !pData->pPage->QueryClose() )
        {
            bRet = false;
            break;
        }
    }
    return bRet;
}



void IconChoiceDialog::Start_Impl()
{
    sal_uInt16 nActPage;

    if ( mnCurrentPageId == 0 || mnCurrentPageId == USHRT_MAX )
        nActPage = maPageList.front()->nId;
    else
        nActPage = mnCurrentPageId;

    // configuration existing?
    //SvtViewOptions aTabDlgOpt( E_TABDIALOG, rId );

    /*if ( aTabDlgOpt.Exists() )
    {
        // possibly position from config
        SetWindowState(OUStringToOString(aTabDlgOpt.GetWindowState().getStr(), RTL_TEXTENCODING_ASCII_US));

        // initial TabPage from program/help/config
        nActPage = (sal_uInt16)aTabDlgOpt.GetPageID();

        if ( USHRT_MAX != mnCurrentPageId )
            nActPage = mnCurrentPageId;

        if ( GetPageData ( nActPage ) == NULL )
            nActPage = maPageList.front()->nId;
    }*/
    //else if ( USHRT_MAX != mnCurrentPageId && GetPageData ( mnCurrentPageId ) != NULL )
    nActPage = mnCurrentPageId;

    mnCurrentPageId = nActPage;

    FocusOnIcon( mnCurrentPageId );

    ActivatePageImpl();
}



const SfxItemSet* IconChoiceDialog::GetRefreshedSet()
{
    SAL_WARN( "cui.dialogs", "GetRefreshedSet not implemented" );
    return 0;
}

/**********************************************************************
|
| tool-methods
|
\**********************************************************************/

IconChoicePageData* IconChoiceDialog::GetPageData ( sal_uInt16 nId )
{
    IconChoicePageData *pRet = NULL;
    for ( size_t i=0; i < maPageList.size(); i++ )
    {
        IconChoicePageData* pData = maPageList[ i ];
        if ( pData->nId == nId )
        {
            pRet = pData;
            break;
        }
    }
    return pRet;
}

/**********************************************************************
|
| OK-Status
|
\**********************************************************************/

bool IconChoiceDialog::OK_Impl()
{
    IconChoicePage* pPage = GetPageData ( mnCurrentPageId )->pPage;

    bool bEnd = !pPage;
    if ( pPage )
    {
        int nRet = IconChoicePage::LEAVE_PAGE;
        if ( pSet )
        {
            SfxItemSet aTmp( *pSet->GetPool(), pSet->GetRanges() );

            if ( pPage->HasExchangeSupport() )
                nRet = pPage->DeactivatePage( &aTmp );

            if ( ( IconChoicePage::LEAVE_PAGE & nRet ) == IconChoicePage::LEAVE_PAGE
                 && aTmp.Count() )
            {
                pExampleSet->Put( aTmp );
                pOutSet->Put( aTmp );
            }
        }
        else
            nRet = pPage->DeactivatePage( NULL );
        bEnd = nRet;
    }

    return bEnd;
}



short IconChoiceDialog::Ok()
{
    bInOK = true;

    if ( !pOutSet )
    {
        if ( !pExampleSet && pSet )
            pOutSet = pSet->Clone( false ); // without items
        else if ( pExampleSet )
            pOutSet = new SfxItemSet( *pExampleSet );
    }
    bool _bModified = false;

    for ( size_t i = 0, nCount = maPageList.size(); i < nCount; ++i )
    {
        IconChoicePageData* pData = GetPageData ( i );

        IconChoicePage* pPage = pData->pPage;

        if ( pPage )
        {
            if ( pData->bOnDemand )
            {
                SfxItemSet& rSet = (SfxItemSet&)pPage->GetItemSet();
                rSet.ClearItem();
                _bModified |= pPage->FillItemSet( &rSet );
            }
            else if ( pSet && !pPage->HasExchangeSupport() )
            {
                SfxItemSet aTmp( *pSet->GetPool(), pSet->GetRanges() );

                if ( pPage->FillItemSet( &aTmp ) )
                {
                    _bModified |= true;
                    pExampleSet->Put( aTmp );
                    pOutSet->Put( aTmp );
                }
            }
        }
    }

    if ( _bModified || ( pOutSet && pOutSet->Count() > 0 ) )
        _bModified |= true;

    return _bModified ? RET_OK : RET_CANCEL;
}



void IconChoiceDialog::FocusOnIcon( sal_uInt16 nId )
{
    // set focus to icon for the current visible page
    for ( sal_uInt16 i=0; i<m_pIconCtrl->GetEntryCount(); i++)
    {
        SvxIconChoiceCtrlEntry* pEntry = m_pIconCtrl->GetEntry ( i );
        sal_uInt16* pUserData = (sal_uInt16*) pEntry->GetUserData();

        if ( pUserData && *pUserData == nId )
        {
            m_pIconCtrl->SetCursor( pEntry );
            break;
        }
    }
}


/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
