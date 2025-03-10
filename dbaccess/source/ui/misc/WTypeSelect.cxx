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

#include "WTypeSelect.hxx"
#include "dbustrings.hrc"
#include <tools/diagnose_ex.h>
#include <osl/diagnose.h>
#include "FieldDescriptions.hxx"
#include "WCopyTable.hxx"
#include "dbaccess_helpid.hrc"
#include "dbu_misc.hrc"
#include <tools/stream.hxx>
#include <svtools/svparser.hxx>
#include "UITools.hxx"
#include "sqlmessage.hxx"
#include "FieldControls.hxx"
#include <vcl/layout.hxx>

#include "dbaccess_slotid.hrc"

using namespace ::dbaui;
using namespace ::com::sun::star::uno;
using namespace ::com::sun::star::beans;
using namespace ::com::sun::star::container;
using namespace ::com::sun::star::util;
using namespace ::com::sun::star::sdbc;

// OWizTypeSelectControl
OWizTypeSelectControl::OWizTypeSelectControl(Window* pParent, Window* pParentTabPage, OTableDesignHelpBar* pHelpBar)
    : OFieldDescControl(pParent, pHelpBar)
{

    m_pParentTabPage = pParentTabPage;
}

OWizTypeSelectControl::~OWizTypeSelectControl()
{

}

void OWizTypeSelectControl::ActivateAggregate( EControlType eType )
{
    switch(eType )
    {
        case tpFormat:
        case tpDefault:
        case tpAutoIncrement:
        case tpAutoIncrementValue:
            break;
        default:
            OFieldDescControl::ActivateAggregate( eType );
    }
}

void OWizTypeSelectControl::DeactivateAggregate( EControlType eType )
{
    switch(eType )
    {
        case tpFormat:
        case tpDefault:
        case tpAutoIncrement:
        case tpAutoIncrementValue:
            break;
        default:
            OFieldDescControl::DeactivateAggregate( eType );
    }
}

void OWizTypeSelectControl::CellModified(long nRow, sal_uInt16 nColId )
{
    OSL_ENSURE(nRow == -1,"nRow muss -1 sein!");
    (void)nRow;

    MultiListBox *pListBox = ((OWizTypeSelect*)m_pParentTabPage)->m_pColumnNames;

    OFieldDescription* pCurFieldDescr = getCurrentFieldDescData();

    sal_uInt16 nPos = pListBox->GetEntryPos( OUString( pCurFieldDescr->GetName() ) );
    pCurFieldDescr = static_cast< OFieldDescription* >( pListBox->GetEntryData( nPos ) );
    OSL_ENSURE( pCurFieldDescr, "OWizTypeSelectControl::CellModified: Columnname/type not found in the listbox!" );
    if ( !pCurFieldDescr )
        return;
    setCurrentFieldDescData( pCurFieldDescr );

    OUString sName = pCurFieldDescr->GetName();
    OUString sNewName;
    const OPropColumnEditCtrl* pColumnName = getColumnCtrl();
    if ( pColumnName )
        sNewName = pColumnName->GetText();

    switch(nColId)
    {
        case FIELD_PROPERTY_COLUMNNAME:
            {
                OCopyTableWizard* pWiz = static_cast<OCopyTableWizard*>(GetParentDialog());
                // first we have to check if this name already exists
                bool bDoubleName = false;
                bool bCase = true;
                if ( getMetaData().is() && !getMetaData()->supportsMixedCaseQuotedIdentifiers() )
                {
                    bCase = false;
                    sal_uInt16 nCount = pListBox->GetEntryCount();
                    for (sal_uInt16 i=0 ; !bDoubleName && i < nCount ; ++i)
                    {
                        OUString sEntry(pListBox->GetEntry(i));
                        bDoubleName = sNewName.equalsIgnoreAsciiCase(sEntry);
                    }
                    if ( !bDoubleName && pWiz->shouldCreatePrimaryKey() )
                        bDoubleName = sNewName.equalsIgnoreAsciiCase(pWiz->getPrimaryKeyName());

                }
                else
                    bDoubleName =  ((pListBox->GetEntryPos(OUString(sNewName)) != LISTBOX_ENTRY_NOTFOUND)
                                    || ( pWiz->shouldCreatePrimaryKey()
                                        &&  pWiz->getPrimaryKeyName() == sNewName) );

                if ( bDoubleName )
                {
                    OUString strMessage = ModuleRes(STR_TABLEDESIGN_DUPLICATE_NAME);
                    strMessage = strMessage.replaceFirst("$column$", sNewName);
                    pWiz->showError(strMessage);
                    pCurFieldDescr->SetName(sName);
                    DisplayData(pCurFieldDescr);
                    static_cast<OWizTypeSelect*>(m_pParentTabPage)->setDuplicateName(true);
                    return;
                }

                OUString sOldName = pCurFieldDescr->GetName();
                pCurFieldDescr->SetName(sNewName);
                static_cast<OWizTypeSelect*>(m_pParentTabPage)->setDuplicateName(false);

                // now we change the name
                OCopyTableWizard::TNameMapping::iterator aIter = pWiz->m_mNameMapping.begin();
                OCopyTableWizard::TNameMapping::iterator aEnd  = pWiz->m_mNameMapping.end();

                ::comphelper::UStringMixEqual aCase(bCase);
                for(;aIter != aEnd;++aIter)
                {
                    if ( aCase(aIter->second,sName) )
                    {
                        aIter->second = sNewName;
                        break;
                    }
                }

                pListBox->RemoveEntry(nPos);
                pListBox->InsertEntry(pCurFieldDescr->GetName(),nPos);
                pListBox->SetEntryData(nPos,pCurFieldDescr);

                pWiz->replaceColumn(nPos,pCurFieldDescr,sOldName);
            }
            break;
    }
    saveCurrentFieldDescData();
}

::com::sun::star::lang::Locale  OWizTypeSelectControl::GetLocale() const
{
    return static_cast<OWizTypeSelect*>(m_pParentTabPage)->m_pParent->GetLocale();
}

Reference< XNumberFormatter > OWizTypeSelectControl::GetFormatter() const
{
    return static_cast<OWizTypeSelect*>(m_pParentTabPage)->m_pParent->GetFormatter();
}

TOTypeInfoSP    OWizTypeSelectControl::getTypeInfo(sal_Int32 _nPos)
{
    return static_cast<OWizTypeSelect*>(m_pParentTabPage)->m_pParent->getDestTypeInfo(_nPos);
}

const OTypeInfoMap* OWizTypeSelectControl::getTypeInfo() const
{
    return &static_cast<OWizTypeSelect*>(m_pParentTabPage)->m_pParent->getDestTypeInfo();
}

::com::sun::star::uno::Reference< ::com::sun::star::sdbc::XDatabaseMetaData> OWizTypeSelectControl::getMetaData()
{
    return ((OWizTypeSelect*)m_pParentTabPage)->m_pParent->m_xDestConnection->getMetaData();
}

::com::sun::star::uno::Reference< ::com::sun::star::sdbc::XConnection> OWizTypeSelectControl::getConnection()
{
    return ((OWizTypeSelect*)m_pParentTabPage)->m_pParent->m_xDestConnection;
}

bool OWizTypeSelectControl::isAutoIncrementValueEnabled() const
{
    return ((OWizTypeSelect*)m_pParentTabPage)->m_bAutoIncrementEnabled;
}

OUString OWizTypeSelectControl::getAutoIncrementValue() const
{
    return ((OWizTypeSelect*)m_pParentTabPage)->m_sAutoIncrementValue;
}

#define IMG_PRIMARY_KEY 1
OWizTypeSelect::OWizTypeSelect( Window* pParent, SvStream* _pStream )
               :OWizardPage( pParent, "TypeSelect", "dbaccess/ui/typeselectpage.ui")
               ,m_pTypeControl(new OWizTypeSelectControl(get<VclVBox>("control_container"), this) )
               ,m_pParserStream( _pStream )
               ,m_nDisplayRow(0)
               ,m_bAutoIncrementEnabled(false)
               ,m_bDuplicateName(false)
{
    get(m_pColumnNames, "columnnames");
    m_pColumnNames->SetParentTabPage(this);
    get(m_pColumns, "columns");
    get(m_pAutoType, "autotype");
    get(m_pAutoFt, "autolabel");
    get(m_pAutoEt, "auto");
    get(m_pAutoPb, "autobutton");

    m_pColumnNames->SetSelectHdl(LINK(this,OWizTypeSelect,ColumnSelectHdl));

    ModuleRes aModuleRes(IMG_JOINS);
    ImageList aImageList(aModuleRes);
    m_imgPKey = aImageList.GetImage(IMG_PRIMARY_KEY);


    m_pTypeControl->Show();
    m_pTypeControl->Init();

    m_pAutoEt->SetText(OUString("10"));
    m_pAutoEt->SetDecimalDigits(0);
    m_pAutoPb->SetClickHdl(LINK(this,OWizTypeSelect,ButtonClickHdl));
    m_pColumnNames->EnableMultiSelection(true);

    try
    {
        m_pColumnNames->SetPKey( m_pParent->supportsPrimaryKey() );
        ::dbaui::fillAutoIncrementValue( m_pParent->m_xDestConnection, m_bAutoIncrementEnabled, m_sAutoIncrementValue );
    }
    catch(const Exception&)
    {
        DBG_UNHANDLED_EXCEPTION();
    }
}

OWizTypeSelect::~OWizTypeSelect()
{
    delete m_pTypeControl;
}

OUString OWizTypeSelect::GetTitle() const
{
    return ModuleRes(STR_WIZ_TYPE_SELECT_TITEL);
}

IMPL_LINK( OWizTypeSelect, ColumnSelectHdl, MultiListBox *, /*pListBox*/ )
{
    OUString aColumnName( m_pColumnNames->GetSelectEntry() );

    OFieldDescription* pField = static_cast<OFieldDescription*>(m_pColumnNames->GetEntryData(m_pColumnNames->GetEntryPos(aColumnName)));
    if(pField)
        m_pTypeControl->DisplayData(pField);

    m_pTypeControl->Enable(m_pColumnNames->GetSelectEntryCount() == 1 );
    return 0;
}

void OWizTypeSelect::Reset()
{
    // restore original state

    while(m_pColumnNames->GetEntryCount())
        m_pColumnNames->RemoveEntry(0);
    m_pColumnNames->Clear();
    sal_Int32 nBreakPos;
    m_pParent->CheckColumns(nBreakPos);

    const ODatabaseExport::TColumnVector& rDestColumns = m_pParent->getDestVector();
    ODatabaseExport::TColumnVector::const_iterator aIter = rDestColumns.begin();
    ODatabaseExport::TColumnVector::const_iterator aEnd = rDestColumns.end();
    for(;aIter != aEnd;++aIter)
    {
        sal_uInt16 nPos;
        if((*aIter)->second->IsPrimaryKey())
            nPos = m_pColumnNames->InsertEntry((*aIter)->first, m_imgPKey );
        else
            nPos = m_pColumnNames->InsertEntry((*aIter)->first);
        m_pColumnNames->SetEntryData(nPos,(*aIter)->second);
    }
    m_bFirstTime = false;
}

void OWizTypeSelect::ActivatePage( )
{
    bool bOldFirstTime = m_bFirstTime;
    Reset();
    m_bFirstTime = bOldFirstTime;

    m_pColumnNames->SelectEntryPos(static_cast<sal_uInt16>(m_nDisplayRow));
    m_nDisplayRow = 0;
    m_pColumnNames->GetSelectHdl().Call(m_pColumnNames);
}

bool OWizTypeSelect::LeavePage()
{
    OUString aColumnName( m_pColumnNames->GetSelectEntry() );

    bool bDuplicateName = false;
    OFieldDescription* pField = static_cast<OFieldDescription*>(m_pColumnNames->GetEntryData(m_pColumnNames->GetEntryPos(aColumnName)));
    if ( pField )
    {
        m_pTypeControl->SaveData(pField);
        bDuplicateName = m_bDuplicateName;
    }
    return !bDuplicateName;
}

void OWizTypeSelect::EnableAuto(bool bEnable)
{
    m_pAutoFt->Show(bEnable);
    m_pAutoEt->Show(bEnable);
    m_pAutoPb->Show(bEnable);
    m_pAutoType->Show(bEnable);
}

IMPL_LINK( OWizTypeSelect, ButtonClickHdl, Button *, /*pButton*/ )
{
    sal_Int32 nBreakPos;
    m_pParent->CheckColumns(nBreakPos);
    fillColumnList(m_pAutoEt->GetText().toInt32());

    ActivatePage();

    return 0;
}

bool OWizTypeSelectList::IsPrimaryKeyAllowed() const
{
    sal_uInt16 nCount = GetSelectEntryCount();
    sal_uInt16 j;

    for( j = 0; m_bPKey && j < nCount; ++j )
    {
        OFieldDescription* pField = static_cast<OFieldDescription*>(GetEntryData(GetSelectEntryPos(j)));
        if(!pField || pField->getTypeInfo()->nSearchType == ColumnSearch::NONE)
            break;
    }
    return j == nCount;
}

void OWizTypeSelectList::setPrimaryKey(OFieldDescription* _pFieldDescr, sal_uInt16 _nPos, bool _bSet)
{
    OUString sColumnName = GetEntry(_nPos);
    RemoveEntry(_nPos);
    _pFieldDescr->SetPrimaryKey(_bSet);
    if( _bSet )
        InsertEntry(sColumnName,((OWizTypeSelect*)m_pParentTabPage)->m_imgPKey,_nPos);
    else if( _pFieldDescr->getTypeInfo()->bNullable )
    {
        _pFieldDescr->SetControlDefault(Any());
        InsertEntry(sColumnName,_nPos);
    }
    SetEntryData(_nPos,_pFieldDescr);
}

extern "C" SAL_DLLPUBLIC_EXPORT Window* SAL_CALL makeOWizTypeSelectList(Window *pParent, VclBuilder::stringmap &)
{
    return new OWizTypeSelectList(pParent);
}

bool OWizTypeSelectList::PreNotify( NotifyEvent& rEvt )
{
    bool nDone = false;
    switch( rEvt.GetType() )
    {
        case EVENT_MOUSEBUTTONDOWN:
        {
            const MouseEvent* pMEvt = rEvt.GetMouseEvent();
            if(pMEvt->IsRight() && !pMEvt->GetModifier())
                nDone = true;
        }
            break;
        case EVENT_COMMAND:
        {
            if(!IsPrimaryKeyAllowed())
                break;

            const CommandEvent* pComEvt = rEvt.GetCommandEvent();
            if(pComEvt->GetCommand() != COMMAND_CONTEXTMENU)
                break;
            // the place, at which was clicked
            Point ptWhere(0,0);
            if (pComEvt->IsMouseEvent())
                ptWhere = pComEvt->GetMousePosPixel();

            PopupMenu aContextMenu(ModuleRes(RID_SBA_RTF_PKEYPOPUP));
            switch( aContextMenu.Execute( this, ptWhere ) )
            {
                case SID_TABLEDESIGN_TABED_PRIMARYKEY:
                {
                    sal_uInt16 nCount = GetEntryCount();
                    for(sal_uInt16 j = 0 ; j < nCount ; ++j)
                    {
                        OFieldDescription* pFieldDescr = static_cast<OFieldDescription*>(GetEntryData(j));
                        if( pFieldDescr )
                        {
                            if(pFieldDescr->IsPrimaryKey() && !IsEntryPosSelected(j))
                                setPrimaryKey(pFieldDescr,j);
                            else if(IsEntryPosSelected(j))
                            {
                                setPrimaryKey(pFieldDescr,j,!pFieldDescr->IsPrimaryKey());
                                SelectEntryPos(j);
                            }
                        }
                    }
                    GetSelectHdl().Call(this);
                }
                break;
            }
            nDone = true;
        }
        break;
    }
    return nDone || MultiListBox::PreNotify(rEvt);
}

void OWizTypeSelect::fillColumnList(sal_uInt32 nRows)
{
    if(m_pParserStream)
    {
        sal_Size nTell = m_pParserStream->Tell(); // might change seek position of stream

        SvParser *pReader = createReader(nRows);
        if(pReader)
        {
            pReader->AddRef();
            pReader->CallParser();
            pReader->ReleaseRef();
        }
        m_pParserStream->Seek(nTell);
    }
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
