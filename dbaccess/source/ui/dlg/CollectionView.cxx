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

#include "CollectionView.hxx"
#include <tools/debug.hxx>
#include <tools/diagnose_ex.h>
#include "moduledbu.hxx"
#include "dbu_dlg.hrc"
#include <comphelper/processfactory.hxx>
#include <comphelper/interaction.hxx>
#include <cppuhelper/exc_hlp.hxx>
#include <toolkit/helper/vclunohelper.hxx>
#include <com/sun/star/lang/XMultiServiceFactory.hpp>
#include <com/sun/star/container/XChild.hpp>
#include <com/sun/star/container/XNameContainer.hpp>
#include <com/sun/star/beans/PropertyValue.hpp>
#include <vcl/msgbox.hxx>
#include "dbustrings.hrc"
#include "UITools.hxx"
#include <com/sun/star/container/XHierarchicalNameContainer.hpp>
#include <com/sun/star/ucb/InteractiveAugmentedIOException.hpp>
#include <com/sun/star/ucb/IOErrorCode.hpp>
#include <com/sun/star/task/InteractionHandler.hpp>
#include <com/sun/star/task/InteractionClassification.hpp>
#include <com/sun/star/sdbc/SQLException.hpp>
#include <com/sun/star/awt/XWindow.hpp>
#include <unotools/viewoptions.hxx>
#include <osl/thread.h>
#include <connectivity/dbexception.hxx>

namespace dbaui
{

using namespace ::com::sun::star::uno;
using namespace ::com::sun::star::ucb;
using namespace ::com::sun::star::lang;
using namespace ::com::sun::star::beans;
using namespace ::com::sun::star::container;
using namespace ::com::sun::star::task;
using namespace ::com::sun::star::sdbc;
using namespace comphelper;
OCollectionView::OCollectionView( Window * pParent
                                 ,const Reference< XContent>& _xContent
                                 ,const OUString& _sDefaultName
                                 ,const ::com::sun::star::uno::Reference< ::com::sun::star::uno::XComponentContext >& _rxContext)
    : ModalDialog( pParent, "CollectionView", "dbaccess/ui/collectionviewdialog.ui")
    , m_xContent(_xContent)
    , m_xContext(_rxContext)
    , m_bCreateForm(true)
{
    get(m_pFTCurrentPath, "currentPathLabel");
    get(m_pNewFolder, "newFolderButton");
    get(m_pUp, "upButton");
    get(m_pView, "viewTreeview");
    get(m_pName, "fileNameEntry");
    get(m_pPB_OK, "ok");

    OSL_ENSURE(m_xContent.is(),"No valid content!");
    m_pView->Initialize(m_xContent,OUString());
    m_pFTCurrentPath->SetStyle( m_pFTCurrentPath->GetStyle() | WB_PATHELLIPSIS );
    initCurrentPath();

    m_pName->SetText(_sDefaultName);
    m_pName->GrabFocus();

    m_pNewFolder->SetStyle( m_pNewFolder->GetStyle() | WB_NOPOINTERFOCUS );
    m_pUp->SetModeImage(Image(ModuleRes(IMG_NAVIGATION_BTN_UP_SC)));
    m_pNewFolder->SetModeImage(Image(ModuleRes(IMG_NAVIGATION_CREATEFOLDER_SC)));

    m_pView->SetDoubleClickHdl( LINK( this, OCollectionView, Dbl_Click_FileView ) );
    m_pView->EnableAutoResize();
    m_pView->EnableDelete(true);
    m_pUp->SetClickHdl( LINK( this, OCollectionView, Up_Click ) );
    m_pNewFolder->SetClickHdl( LINK( this, OCollectionView, NewFolder_Click ) );
    m_pPB_OK->SetClickHdl( LINK( this, OCollectionView, Save_Click ) );
}

OCollectionView::~OCollectionView( )
{
}


IMPL_LINK_NOARG(OCollectionView, Save_Click)
{
    OUString sName = m_pName->GetText();
    if ( sName.isEmpty() )
        return 0;
    try
    {
        OUString sSubFolder = m_pView->GetCurrentURL();
        sal_Int32 nIndex = sName.lastIndexOf('/') + 1;
        if ( nIndex )
        {
            if ( nIndex == 1 ) // special handling for root
            {
                Reference<XChild> xChild(m_xContent,UNO_QUERY);
                Reference<XNameAccess> xNameAccess(xChild,UNO_QUERY);
                while( xNameAccess.is() )
                {
                    xNameAccess.set(xChild->getParent(),UNO_QUERY);
                    if ( xNameAccess.is() )
                    {
                        m_xContent.set(xNameAccess,UNO_QUERY);
                        xChild.set(m_xContent,UNO_QUERY);
                    }
                }
                m_pView->Initialize(m_xContent,OUString());
                initCurrentPath();
            }
            sSubFolder = sName.copy(0,nIndex-1);
            sName = sName.copy(nIndex);
            Reference<XHierarchicalNameContainer> xHier(m_xContent,UNO_QUERY);
            OSL_ENSURE(xHier.is(),"XHierarchicalNameContainer not supported!");
            if ( !sSubFolder.isEmpty() && xHier.is() )
            {
                if ( xHier->hasByHierarchicalName(sSubFolder) )
                {
                    m_xContent.set(xHier->getByHierarchicalName(sSubFolder),UNO_QUERY);
                }
                else // sub folder doesn't exist
                {
                    Sequence< Any > aValues(2);
                    PropertyValue aValue;
                    aValue.Name = "ResourceName";
                    aValue.Value <<= sSubFolder;
                    aValues[0] <<= aValue;

                    aValue.Name = "ResourceType";
                    aValue.Value <<= OUString("folder");
                    aValues[1] <<= aValue;

                    InteractionClassification eClass = InteractionClassification_ERROR;
                    ::com::sun::star::ucb::IOErrorCode eError = IOErrorCode_NOT_EXISTING_PATH;
                    OUString sTemp;
                    InteractiveAugmentedIOException aException(sTemp,Reference<XInterface>(),eClass,eError,aValues);

                    Reference<XInteractionHandler2> xHandler(
                        InteractionHandler::createWithParent(m_xContext, VCLUnoHelper::GetInterface( this )));
                    OInteractionRequest* pRequest = new OInteractionRequest(makeAny(aException));
                    Reference< XInteractionRequest > xRequest(pRequest);

                    OInteractionApprove* pApprove = new OInteractionApprove;
                    pRequest->addContinuation(pApprove);
                    xHandler->handle(xRequest);

                    return 0;
                }
            }
        }
        Reference<XNameContainer> xNameContainer(m_xContent,UNO_QUERY);
        if ( xNameContainer.is() )
        {
            Reference< XContent> xContent;
            if ( xNameContainer->hasByName(sName) )
            {
                QueryBox aBox( this, WB_YES_NO, ModuleRes( STR_ALREADYEXISTOVERWRITE ) );
                if ( aBox.Execute() != RET_YES )
                    return 0;
            }
            m_pName->SetText(sName);
            EndDialog( sal_True );
        }
    }
    catch( const Exception& )
    {
        DBG_UNHANDLED_EXCEPTION();
    }
    return 0;
}

IMPL_LINK_NOARG(OCollectionView, NewFolder_Click)
{
    try
    {
        Reference<XHierarchicalNameContainer> xNameContainer(m_xContent,UNO_QUERY);
        if ( dbaui::insertHierachyElement(this,m_xContext,xNameContainer,OUString(),m_bCreateForm) )
            m_pView->Initialize(m_xContent,OUString());
    }
    catch( const SQLException& )
    {
        showError( ::dbtools::SQLExceptionInfo( ::cppu::getCaughtException() ), this, m_xContext );
    }
    catch( const Exception& )
    {
        DBG_UNHANDLED_EXCEPTION();
    }
    return 0;
}

IMPL_LINK_NOARG(OCollectionView, Up_Click)
{
    try
    {
        Reference<XChild> xChild(m_xContent,UNO_QUERY);
        if ( xChild.is() )
        {
            Reference<XNameAccess> xNameAccess(xChild->getParent(),UNO_QUERY);
            if ( xNameAccess.is() )
            {
                m_xContent.set(xNameAccess,UNO_QUERY);
                m_pView->Initialize(m_xContent,OUString());
                initCurrentPath();
            }
            else
                m_pUp->Disable();
        }
    }
    catch( const Exception& )
    {
        DBG_UNHANDLED_EXCEPTION();
    }
    return 0;
}

IMPL_LINK_NOARG(OCollectionView, Dbl_Click_FileView)
{
    try
    {
        Reference<XNameAccess> xNameAccess(m_xContent,UNO_QUERY);
        if ( xNameAccess.is() )
        {
            OUString sSubFolder = m_pView->GetCurrentURL();
            sal_Int32 nIndex = sSubFolder.lastIndexOf('/') + 1;
            sSubFolder = sSubFolder.getToken(0,'/',nIndex);
            if ( !sSubFolder.isEmpty() )
            {
                Reference< XContent> xContent;
                if ( xNameAccess->hasByName(sSubFolder) )
                    xContent.set(xNameAccess->getByName(sSubFolder),UNO_QUERY);
                if ( xContent.is() )
                {
                    m_xContent = xContent;
                    m_pView->Initialize(m_xContent,OUString());
                    initCurrentPath();
                }
            }
        }
    }
    catch( const Exception& )
    {
        DBG_UNHANDLED_EXCEPTION();
    }
    return 0;
}

void OCollectionView::initCurrentPath()
{
    bool bEnable = false;
    try
    {
        if ( m_xContent.is() )
        {
            const OUString sCID = m_xContent->getIdentifier()->getContentIdentifier();
            const static OUString s_sFormsCID("private:forms");
            const static OUString s_sReportsCID("private:reports");
            m_bCreateForm = s_sFormsCID == sCID ;
            OUString sPath("/");
            if ( m_bCreateForm && sCID.getLength() != s_sFormsCID.getLength())
                sPath = sCID.copy(s_sFormsCID.getLength());
            else if ( !m_bCreateForm && sCID.getLength() != s_sReportsCID.getLength() )
                sPath = sCID.copy(s_sReportsCID.getLength() - 2);

            m_pFTCurrentPath->SetText(sPath);
            Reference<XChild> xChild(m_xContent,UNO_QUERY);
            bEnable = xChild.is() && Reference<XNameAccess>(xChild->getParent(),UNO_QUERY).is();
        }
    }
    catch( const Exception& )
    {
        DBG_UNHANDLED_EXCEPTION();
    }
    m_pUp->Enable(bEnable);
}

OUString OCollectionView::getName() const
{
    return m_pName->GetText();
}

}   // namespace dbaui

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
