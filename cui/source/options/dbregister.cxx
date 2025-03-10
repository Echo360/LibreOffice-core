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

#include "dbregister.hxx"
#include "dbregistersettings.hxx"
#include "connpooloptions.hxx"
#include <svl/filenotation.hxx>
#include "helpid.hrc"
#include <svtools/editbrowsebox.hxx>
#include "svtools/treelistentry.hxx"
#include <cuires.hrc>
#include <vcl/field.hxx>
#include <vcl/layout.hxx>
#include <svl/eitem.hxx>
#include <comphelper/processfactory.hxx>
#include <com/sun/star/uno/Exception.hpp>
#include <com/sun/star/lang/XMultiServiceFactory.hpp>
#include <com/sun/star/ui/dialogs/XFilePicker.hpp>
#include <com/sun/star/ui/dialogs/XFilterManager.hpp>
#include <com/sun/star/ui/dialogs/ExecutableDialogResults.hpp>
#include <vcl/msgbox.hxx>
#include <svtools/svtabbx.hxx>
#include <svl/itemset.hxx>
#include "doclinkdialog.hxx"
#include <unotools/localfilehelper.hxx>
#include "optHeaderTabListbox.hxx"
#include <sfx2/docfilt.hxx>
#include <dialmgr.hxx>
#include "dbregisterednamesconfig.hxx"
#include <svx/dialogs.hrc>

#define TAB_WIDTH1      80
#define TAB_WIDTH_MIN   10
#define TAB_WIDTH2      1000
#define ITEMID_TYPE       1
#define ITEMID_PATH       2

namespace svx
{


using namespace ::com::sun::star::lang;
using namespace ::com::sun::star::ui::dialogs;
using namespace ::com::sun::star::uno;
using namespace ::svt;

// class RegistrationItemSetHolder  -------------------------------------------------

RegistrationItemSetHolder::RegistrationItemSetHolder( const SfxItemSet& _rMasterSet )
    :m_aRegistrationItems( _rMasterSet )
{
    DbRegisteredNamesConfig::GetOptions( m_aRegistrationItems );
}

RegistrationItemSetHolder::~RegistrationItemSetHolder()
{
}

// class DatabaseRegistrationDialog  ------------------------------------------------

DatabaseRegistrationDialog::DatabaseRegistrationDialog( Window* pParent, const SfxItemSet& rInAttrs )
    : RegistrationItemSetHolder(rInAttrs)
    , SfxSingleTabDialog(pParent, getRegistrationItems())
{
    SfxTabPage* page = DbRegistrationOptionsPage::Create(get_content_area(), &getRegistrationItems());
    SetTabPage(page);
    SetText(page->get<VclFrame>("frame1")->get_label());
}

DatabaseRegistrationDialog::~DatabaseRegistrationDialog()
{
}

short DatabaseRegistrationDialog::Execute()
{
    short result = SfxSingleTabDialog::Execute();
    if ( result == RET_OK )
    {
        DBG_ASSERT( GetOutputItemSet(), "DatabaseRegistrationDialog::Execute: no output items!" );
        if ( GetOutputItemSet() )
            DbRegisteredNamesConfig::SetOptions( *GetOutputItemSet() );
    }
    return result;
}

// class DbRegistrationOptionsPage --------------------------------------------------

DbRegistrationOptionsPage::DbRegistrationOptionsPage( Window* pParent, const SfxItemSet& rSet ) :

    SfxTabPage( pParent, "DbRegisterPage", "cui/ui/dbregisterpage.ui", &rSet ),

    aTypeText       ( CUI_RES( RID_SVXSTR_TYPE ) ),
    aPathText       ( CUI_RES( RID_SVXSTR_PATH ) ),
    pPathBox        ( NULL ),
    m_pCurEntry     ( NULL ),
    m_nOldCount     ( 0 ),
    m_bModified     ( false )
{
    get(m_pPathCtrl, "pathctrl");
    Size aControlSize(248, 147);
    aControlSize = LogicToPixel(aControlSize, MAP_APPFONT);
    m_pPathCtrl->set_width_request(aControlSize.Width());
    m_pPathCtrl->set_height_request(aControlSize.Height());

    get(m_pNew, "new");
    get(m_pEdit, "edit");
    get(m_pDelete, "delete");

    m_pNew->SetClickHdl( LINK( this, DbRegistrationOptionsPage, NewHdl ) );
    m_pEdit->SetClickHdl( LINK( this, DbRegistrationOptionsPage, EditHdl ) );
    m_pDelete->SetClickHdl( LINK( this, DbRegistrationOptionsPage, DeleteHdl ) );

    Size aBoxSize = m_pPathCtrl->GetOutputSizePixel();

    WinBits nBits = WB_SORT | WB_HSCROLL | WB_CLIPCHILDREN | WB_TABSTOP;
    pPathBox = new ::svx::OptHeaderTabListBox( *m_pPathCtrl, nBits );

    HeaderBar &rBar = pPathBox->GetTheHeaderBar();

    rBar.SetSelectHdl( LINK( this, DbRegistrationOptionsPage, HeaderSelect_Impl ) );
    rBar.SetEndDragHdl( LINK( this, DbRegistrationOptionsPage, HeaderEndDrag_Impl ) );
    Size aSz;
    aSz.Width() = TAB_WIDTH1;
    rBar.InsertItem( ITEMID_TYPE, aTypeText,
                            LogicToPixel( aSz, MapMode( MAP_APPFONT ) ).Width(),
                            HIB_LEFT | HIB_VCENTER | HIB_CLICKABLE | HIB_UPARROW );
    aSz.Width() = TAB_WIDTH2;
    rBar.InsertItem( ITEMID_PATH, aPathText,
                            LogicToPixel( aSz, MapMode( MAP_APPFONT ) ).Width(),
                            HIB_LEFT | HIB_VCENTER );

    static long aTabs[] = {3, 0, TAB_WIDTH1, TAB_WIDTH1 + TAB_WIDTH2 };
    Size aHeadSize = rBar.GetSizePixel();

    pPathBox->SetStyle( pPathBox->GetStyle()|nBits );
    pPathBox->SetDoubleClickHdl( LINK( this, DbRegistrationOptionsPage, EditHdl ) );
    pPathBox->SetSelectHdl( LINK( this, DbRegistrationOptionsPage, PathSelect_Impl ) );
    pPathBox->SetSelectionMode( SINGLE_SELECTION );
    pPathBox->SetPosSizePixel( Point( 0, aHeadSize.Height() ),
                               Size( aBoxSize.Width(), aBoxSize.Height() - aHeadSize.Height() ) );
    pPathBox->SvSimpleTable::SetTabs( aTabs, MAP_APPFONT );
    pPathBox->SetHighlightRange();

    pPathBox->SetHelpId( HID_DBPATH_CTL_PATH );
    rBar.SetHelpId( HID_DBPATH_HEADERBAR );

    pPathBox->ShowTable();
}



DbRegistrationOptionsPage::~DbRegistrationOptionsPage()
{
    for ( sal_uInt16 i = 0; i < pPathBox->GetEntryCount(); ++i )
        delete static_cast< DatabaseRegistration* >( pPathBox->GetEntry(i)->GetUserData() );
    delete pPathBox;
}



SfxTabPage* DbRegistrationOptionsPage::Create( Window* pParent,
                                    const SfxItemSet* rAttrSet )
{
    return ( new DbRegistrationOptionsPage( pParent, *rAttrSet ) );
}



bool DbRegistrationOptionsPage::FillItemSet( SfxItemSet* rCoreSet )
{
    // the settings for the single drivers
    bool bModified = false;
    DatabaseRegistrations aRegistrations;
    sal_uLong nCount = pPathBox->GetEntryCount();
    for ( sal_uLong i = 0; i < nCount; ++i )
    {
        SvTreeListEntry* pEntry = pPathBox->GetEntry(i);
        DatabaseRegistration* pRegistration = static_cast< DatabaseRegistration* >( pEntry->GetUserData() );
        if ( pRegistration && !pRegistration->sLocation.isEmpty() )
        {
            OUString sName( pPathBox->GetEntryText( pEntry, 0 ) );
            OFileNotation aTransformer( pRegistration->sLocation );
            aRegistrations[ sName ] = DatabaseRegistration( aTransformer.get( OFileNotation::N_URL ), pRegistration->bReadOnly );
        }
    }
    if ( m_nOldCount != aRegistrations.size() || m_bModified )
    {
        rCoreSet->Put(DatabaseMapItem( SID_SB_DB_REGISTER, aRegistrations ), SID_SB_DB_REGISTER);
        bModified = true;
    }

    return bModified;
}



void DbRegistrationOptionsPage::Reset( const SfxItemSet* rSet )
{
    // the settings for the single drivers
    SFX_ITEMSET_GET( *rSet, pRegistrations, DatabaseMapItem, SID_SB_DB_REGISTER, true );
    if ( !pRegistrations )
        return;

    pPathBox->Clear();

    const DatabaseRegistrations& rRegistrations = pRegistrations->getRegistrations();
    m_nOldCount = rRegistrations.size();
    DatabaseRegistrations::const_iterator aIter = rRegistrations.begin();
    DatabaseRegistrations::const_iterator aEnd = rRegistrations.end();
    for ( ; aIter != aEnd; ++aIter )
    {
        OFileNotation aTransformer( aIter->second.sLocation );
        insertNewEntry( aIter->first, aTransformer.get( OFileNotation::N_SYSTEM ), aIter->second.bReadOnly );
    }

    OUString aUserData = GetUserData();
    if ( !aUserData.isEmpty() )
    {
        HeaderBar &rBar = pPathBox->GetTheHeaderBar();

        // restore column width
        rBar.SetItemSize( ITEMID_TYPE, aUserData.getToken(0, ';').toInt32() );
        HeaderEndDrag_Impl( &rBar );
        // restore sort direction
        bool bUp = aUserData.getToken(1, ';').toInt32() != 0;
        HeaderBarItemBits nBits = rBar.GetItemBits(ITEMID_TYPE);

        if ( bUp )
        {
            nBits &= ~HIB_UPARROW;
            nBits |= HIB_DOWNARROW;
        }
        else
        {
            nBits &= ~HIB_DOWNARROW;
            nBits |= HIB_UPARROW;
        }
        rBar.SetItemBits( ITEMID_TYPE, nBits );
        HeaderSelect_Impl( &rBar );
    }
}



void DbRegistrationOptionsPage::FillUserData()
{
    HeaderBar &rBar = pPathBox->GetTheHeaderBar();

    OUString aUserData = OUString::number( rBar.GetItemSize( ITEMID_TYPE ) ) + ";";
    HeaderBarItemBits nBits = rBar.GetItemBits( ITEMID_TYPE );
    bool bUp = ( ( nBits & HIB_UPARROW ) == HIB_UPARROW );
    aUserData += (bUp ? OUString("1") : OUString("0"));
    SetUserData( aUserData );
}


IMPL_LINK_NOARG(DbRegistrationOptionsPage, DeleteHdl)
{
    SvTreeListEntry* pEntry = pPathBox->FirstSelected();
    if ( pEntry )
    {
        QueryBox aQuery(this,CUI_RES(QUERY_DELETE_CONFIRM));
        if ( aQuery.Execute() == RET_YES )
            pPathBox->GetModel()->Remove(pEntry);
    }
    return 0;
}

IMPL_LINK_NOARG(DbRegistrationOptionsPage, NewHdl)
{
    OUString sNewName,sNewLocation;
    openLinkDialog(sNewName,sNewLocation);
    return 0;
}



IMPL_LINK_NOARG(DbRegistrationOptionsPage, EditHdl)
{
    SvTreeListEntry* pEntry = pPathBox->GetCurEntry();
    if ( !pEntry )
        return 0L;

    DatabaseRegistration* pOldRegistration = static_cast< DatabaseRegistration* >( pEntry->GetUserData() );
    if ( !pOldRegistration || pOldRegistration->bReadOnly )
        return 0L;

    OUString sOldName = pPathBox->GetEntryText(pEntry,0);
    m_pCurEntry = pEntry;
    openLinkDialog( sOldName, pOldRegistration->sLocation, pEntry );
    m_pCurEntry = NULL;

    return 1L;
}



IMPL_LINK( DbRegistrationOptionsPage, HeaderSelect_Impl, HeaderBar*, pBar )
{
    assert(pBar);

    if (!pBar || pBar->GetCurItemId() != ITEMID_TYPE)
        return 0;

    HeaderBarItemBits nBits = pBar->GetItemBits(ITEMID_TYPE);
    bool bUp = ( ( nBits & HIB_UPARROW ) == HIB_UPARROW );
    SvSortMode eMode = SortAscending;

    if ( bUp )
    {
        nBits &= ~HIB_UPARROW;
        nBits |= HIB_DOWNARROW;
        eMode = SortDescending;
    }
    else
    {
        nBits &= ~HIB_DOWNARROW;
        nBits |= HIB_UPARROW;
    }
    pBar->SetItemBits( ITEMID_TYPE, nBits );
    SvTreeList* pModel = pPathBox->GetModel();
    pModel->SetSortMode( eMode );
    pModel->Resort();
    return 1;
}



IMPL_LINK( DbRegistrationOptionsPage, HeaderEndDrag_Impl, HeaderBar*, pBar )
{
    assert(pBar);

    if (!pBar || !pBar->GetCurItemId())
        return 0;

    if ( !pBar->IsItemMode() )
    {
        Size aSz;
        sal_uInt16 nTabs = pBar->GetItemCount();
        long nTmpSz = 0;
        long nWidth = pBar->GetItemSize(ITEMID_TYPE);
        long nBarWidth = pBar->GetSizePixel().Width();

        if(nWidth < TAB_WIDTH_MIN)
            pBar->SetItemSize( ITEMID_TYPE, TAB_WIDTH_MIN);
        else if ( ( nBarWidth - nWidth ) < TAB_WIDTH_MIN )
            pBar->SetItemSize( ITEMID_TYPE, nBarWidth - TAB_WIDTH_MIN );

        for ( sal_uInt16 i = 1; i <= nTabs; ++i )
        {
            long _nWidth = pBar->GetItemSize(i);
            aSz.Width() =  _nWidth + nTmpSz;
            nTmpSz += _nWidth;
            pPathBox->SetTab( i, PixelToLogic( aSz, MapMode(MAP_APPFONT) ).Width(), MAP_APPFONT );
        }
    }
    return 1;
}


IMPL_LINK_NOARG(DbRegistrationOptionsPage, PathSelect_Impl)
{
    SvTreeListEntry* pEntry = pPathBox->FirstSelected();

    bool bReadOnly = true;
    if ( pEntry )
    {
        DatabaseRegistration* pRegistration = static_cast< DatabaseRegistration* >( pEntry->GetUserData() );
        bReadOnly = pRegistration->bReadOnly;
    }

    m_pEdit->Enable( !bReadOnly );
    m_pDelete->Enable( !bReadOnly );
    return 0;
}

void DbRegistrationOptionsPage::insertNewEntry( const OUString& _sName,const OUString& _sLocation, const bool _bReadOnly )
{
    OUString aStr( _sName );
    aStr += "\t";
    aStr += _sLocation;

    SvTreeListEntry* pEntry = NULL;
    if ( _bReadOnly )
    {
        Image aLocked( CUI_RES( RID_SVXBMP_LOCK ) );
        pEntry = pPathBox->InsertEntry( aStr, aLocked, aLocked );
    }
    else
    {
        pEntry = pPathBox->InsertEntry( aStr );
    }

    pEntry->SetUserData( new DatabaseRegistration( _sLocation, _bReadOnly ) );
}


void DbRegistrationOptionsPage::openLinkDialog(const OUString& _sOldName,const OUString& _sOldLocation,SvTreeListEntry* _pEntry)
{
    ODocumentLinkDialog aDlg(this,_pEntry == NULL);

    aDlg.setLink(_sOldName,_sOldLocation);
    aDlg.setNameValidator(LINK( this, DbRegistrationOptionsPage, NameValidator ) );

    if ( aDlg.Execute() == RET_OK )
    {
        OUString sNewName,sNewLocation;
        aDlg.getLink(sNewName,sNewLocation);
        if ( _pEntry == NULL || sNewName != _sOldName || sNewLocation != _sOldLocation )
        {
            if ( _pEntry )
            {
                delete static_cast< DatabaseRegistration* >( _pEntry->GetUserData() );
                pPathBox->GetModel()->Remove( _pEntry );
            }
            insertNewEntry( sNewName, sNewLocation, false );
            m_bModified = true;
        }
    }
}

IMPL_LINK( DbRegistrationOptionsPage, NameValidator, OUString*, _pName )
{
    if ( _pName )
    {
        sal_uLong nCount = pPathBox->GetEntryCount();
        for ( sal_uLong i = 0; i < nCount; ++i )
        {
            SvTreeListEntry* pEntry = pPathBox->GetEntry(i);
            if ( (!m_pCurEntry || m_pCurEntry != pEntry) && pPathBox->GetEntryText(pEntry,0) == *_pName )
                return 0L;
        }
    }
    return 1L;
}

}   // namespace svx



/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
