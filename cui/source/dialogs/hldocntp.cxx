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

#include "hldocntp.hxx"
#include <sfx2/viewfrm.hxx>
#include <sfx2/docfac.hxx>
#include <com/sun/star/uno/Reference.h>
#include <com/sun/star/uno/Sequence.h>
#include <com/sun/star/beans/PropertyValue.hpp>
#include <com/sun/star/uno/Exception.hpp>
#include <unotools/localfilehelper.hxx>
#include <vcl/image.hxx>
#include <tools/urlobj.hxx>
#include <unotools/pathoptions.hxx>
#include <unotools/dynamicmenuoptions.hxx>
#include <sfx2/filedlghelper.hxx>
#include <unotools/ucbstreamhelper.hxx>
#include <unotools/ucbhelper.hxx>

#include <comphelper/processfactory.hxx>
#include <com/sun/star/lang/XMultiServiceFactory.hpp>
#include <com/sun/star/ui/dialogs/FolderPicker.hpp>
#include <com/sun/star/ui/dialogs/ExecutableDialogResults.hpp>

using namespace ::com::sun::star::lang;
using namespace ::com::sun::star::ui::dialogs;
using namespace ::com::sun::star::uno;

using namespace ::rtl;
using namespace ::com::sun::star;

/*************************************************************************
|*
|* Data-struct for documenttypes in listbox
|*
|************************************************************************/

struct DocumentTypeData
{
    OUString aStrURL;
    OUString aStrExt;
    DocumentTypeData (const OUString& aURL, const OUString& aExt) : aStrURL(aURL), aStrExt(aExt)
    {}
};

bool SvxHyperlinkNewDocTp::ImplGetURLObject( const OUString& rPath, const OUString& rBase, INetURLObject& aURLObject ) const
{
    bool bIsValidURL = !rPath.isEmpty();
    if ( bIsValidURL )
    {
        aURLObject.SetURL( rPath );
        if ( aURLObject.GetProtocol() == INET_PROT_NOT_VALID )      // test if the source is already a valid url
        {                                                           // if not we have to create a url from a physical file name
            bool wasAbs;
            INetURLObject base(rBase);
            base.setFinalSlash();
            aURLObject = base.smartRel2Abs(
                rPath, wasAbs, true, INetURLObject::ENCODE_ALL,
                RTL_TEXTENCODING_UTF8, true);
        }
        bIsValidURL = aURLObject.GetProtocol() != INET_PROT_NOT_VALID;
        if ( bIsValidURL )
        {
            OUString aBase( aURLObject.getName( INetURLObject::LAST_SEGMENT, false ) );
            if ( aBase.isEmpty() || ( aBase[0] == '.' ) )
                bIsValidURL = false;
        }
        if ( bIsValidURL )
        {
            sal_Int32 nPos = m_pLbDocTypes->GetSelectEntryPos();
            if ( nPos != LISTBOX_ENTRY_NOTFOUND )
                aURLObject.SetExtension( ((DocumentTypeData*)m_pLbDocTypes->GetEntryData( nPos ))->aStrExt );
        }

    }
    return bIsValidURL;
}

/*************************************************************************
|*
|* Contructor / Destructor
|*
|************************************************************************/

SvxHyperlinkNewDocTp::SvxHyperlinkNewDocTp ( Window *pParent, IconChoiceDialog* pDlg, const SfxItemSet& rItemSet)
:   SvxHyperlinkTabPageBase ( pParent, pDlg, "HyperlinkNewDocPage", "cui/ui/hyperlinknewdocpage.ui", rItemSet )
{
    get(m_pRbtEditNow, "editnow");
    get(m_pRbtEditLater, "editlater");
    get(m_pCbbPath, "path");
    m_pCbbPath->SetSmartProtocol(INET_PROT_FILE);
    get(m_pBtCreate, "create");
    m_pBtCreate->SetModeImage(Image(CUI_RES(RID_SVXBMP_NEWDOC)));
    get(m_pLbDocTypes, "types");
    m_pLbDocTypes->set_height_request(m_pLbDocTypes->GetTextHeight() * 5);

    // Set HC bitmaps and disable display of bitmap names.
    m_pBtCreate->EnableTextDisplay (false);

    InitStdControls();

    SetExchangeSupport ();

    m_pCbbPath->Show();
    m_pCbbPath->SetBaseURL(SvtPathOptions().GetWorkPath());

    // set defaults
    m_pRbtEditNow->Check();

    m_pBtCreate->SetClickHdl        ( LINK ( this, SvxHyperlinkNewDocTp, ClickNewHdl_Impl ) );

    FillDocumentList ();
}

SvxHyperlinkNewDocTp::~SvxHyperlinkNewDocTp ()
{
    for ( sal_uInt16 n=0; n<m_pLbDocTypes->GetEntryCount(); n++ )
    {
        DocumentTypeData* pTypeData = (DocumentTypeData*)
                                      m_pLbDocTypes->GetEntryData ( n );
        delete pTypeData;
    }
}

/*************************************************************************
|*
|* Fill the all dialog-controls except controls in groupbox "more..."
|*
|************************************************************************/


void SvxHyperlinkNewDocTp::FillDlgFields(const OUString& /*rStrURL*/)
{
}

void SvxHyperlinkNewDocTp::FillDocumentList ()
{
    EnterWait();

    uno::Sequence< uno::Sequence< beans::PropertyValue > >
        aDynamicMenuEntries( SvtDynamicMenuOptions().GetMenu( E_NEWMENU ) );

    sal_uInt32 i, nCount = aDynamicMenuEntries.getLength();
    for ( i = 0; i < nCount; i++ )
    {
        uno::Sequence< beans::PropertyValue >& rDynamicMenuEntry = aDynamicMenuEntries[ i ];

        OUString aDocumentUrl, aTitle, aImageId, aTargetName;

           for ( int e = 0; e < rDynamicMenuEntry.getLength(); e++ )
        {
            if ( rDynamicMenuEntry[ e ].Name == DYNAMICMENU_PROPERTYNAME_URL )
                rDynamicMenuEntry[ e ].Value >>= aDocumentUrl;
            else if ( rDynamicMenuEntry[e].Name == DYNAMICMENU_PROPERTYNAME_TITLE )
                rDynamicMenuEntry[e].Value >>= aTitle;
            else if ( rDynamicMenuEntry[e].Name == DYNAMICMENU_PROPERTYNAME_IMAGEIDENTIFIER )
                rDynamicMenuEntry[e].Value >>= aImageId;
            else if ( rDynamicMenuEntry[e].Name == DYNAMICMENU_PROPERTYNAME_TARGETNAME )
                rDynamicMenuEntry[e].Value >>= aTargetName;
        }
        //#i96822# business cards, labels and database should not be inserted here
        if( aDocumentUrl == "private:factory/swriter?slot=21051" ||
            aDocumentUrl == "private:factory/swriter?slot=21052" ||
            aDocumentUrl == "private:factory/sdatabase?Interactive" )
            continue;

        // Insert into listbox
        if ( !aDocumentUrl.isEmpty() )
        {
            if ( aDocumentUrl == "private:factory/simpress?slot=6686" )              // SJ: #106216# do not start
                aDocumentUrl = "private:factory/simpress"; // the AutoPilot for impress

            // insert private-url and default-extension as user-data
            const SfxFilter* pFilter = SfxFilter::GetDefaultFilterFromFactory( aDocumentUrl );
            if ( pFilter )
            {
                // insert doc-name and image
                OUString aTitleName( aTitle );
                aTitleName = aTitleName.replaceFirst( "~", "" );

                sal_Int16 nPos = m_pLbDocTypes->InsertEntry ( aTitleName );
                OUString aStrDefExt( pFilter->GetDefaultExtension () );
                DocumentTypeData *pTypeData = new DocumentTypeData ( aDocumentUrl, aStrDefExt.copy( 2 ) );
                m_pLbDocTypes->SetEntryData ( nPos, pTypeData );
            }
        }
    }
    m_pLbDocTypes->SelectEntryPos ( 0 );

    LeaveWait();
}

/*************************************************************************
|*
|* retrieve and prepare data from dialog-fields
|*
|************************************************************************/

void SvxHyperlinkNewDocTp::GetCurentItemData ( OUString& rStrURL, OUString& aStrName,
                                               OUString& aStrIntName, OUString& aStrFrame,
                                               SvxLinkInsertMode& eMode )
{
    // get data from dialog-controls
    rStrURL = m_pCbbPath->GetText();
    INetURLObject aURL;
    if ( ImplGetURLObject( rStrURL, m_pCbbPath->GetBaseURL(), aURL ) )
    {
        rStrURL = aURL.GetMainURL( INetURLObject::NO_DECODE );
    }

    GetDataFromCommonFields( aStrName, aStrIntName, aStrFrame, eMode );
}

/*************************************************************************
|*
|* static method to create Tabpage
|*
|************************************************************************/

IconChoicePage* SvxHyperlinkNewDocTp::Create( Window* pWindow, IconChoiceDialog* pDlg, const SfxItemSet& rItemSet )
{
    return( new SvxHyperlinkNewDocTp( pWindow, pDlg, rItemSet ) );
}

/*************************************************************************
|*
|* Set initial focus
|*
|************************************************************************/

void SvxHyperlinkNewDocTp::SetInitFocus()
{
    m_pCbbPath->GrabFocus();
}

/*************************************************************************
|*
|* Ask page whether an insert is possible
|*
\************************************************************************/

bool SvxHyperlinkNewDocTp::AskApply()
{
    INetURLObject aINetURLObject;
    bool bRet = ImplGetURLObject( m_pCbbPath->GetText(), m_pCbbPath->GetBaseURL(), aINetURLObject );
    if ( !bRet )
    {
        WarningBox aWarning( this, WB_OK, CUI_RESSTR(RID_SVXSTR_HYPDLG_NOVALIDFILENAME) );
        aWarning.Execute();
    }
    return bRet;
}

/*************************************************************************
|*
|* Any action to do after apply-button is pressed
|*
\************************************************************************/

void SvxHyperlinkNewDocTp::DoApply ()
{
    EnterWait();

    // get data from dialog-controls
    OUString aStrNewName = m_pCbbPath->GetText();

    if ( aStrNewName == aEmptyStr )
        aStrNewName = maStrInitURL;


    // create a real URL-String

    INetURLObject aURL;
    if ( ImplGetURLObject( aStrNewName, m_pCbbPath->GetBaseURL(), aURL ) )
    {


        // create Document

        aStrNewName = aURL.GetURLPath( INetURLObject::NO_DECODE );
        SfxViewFrame *pViewFrame = NULL;
        try
        {
            bool bCreate = true;

            // check if file exists, warn before we overwrite it
            {
                com::sun::star::uno::Reference < com::sun::star::task::XInteractionHandler > xHandler;
                SvStream* pIStm = ::utl::UcbStreamHelper::CreateStream( aURL.GetMainURL( INetURLObject::NO_DECODE ), STREAM_READ, xHandler );

                bool bOk = pIStm && ( pIStm->GetError() == 0);

                if( pIStm )
                    delete pIStm;

                if( bOk )
                {
                    WarningBox aWarning( this, WB_YES_NO, CUI_RESSTR(RID_SVXSTR_HYPERDLG_QUERYOVERWRITE) );
                    bCreate = aWarning.Execute() == BUTTON_YES;
                }
            }

            if( bCreate )
            {
                // current document
                SfxViewFrame* pCurrentDocFrame = SfxViewFrame::Current();

                if ( aStrNewName != aEmptyStr )
                {
                    // get private-url
                    sal_Int32 nPos = m_pLbDocTypes->GetSelectEntryPos();
                    if( nPos == LISTBOX_ENTRY_NOTFOUND )
                        nPos=0;
                    OUString aStrDocName ( ( ( DocumentTypeData* )
                                         m_pLbDocTypes->GetEntryData( nPos ) )->aStrURL );

                    // create items
                    SfxStringItem aName( SID_FILE_NAME, aStrDocName );
                    SfxStringItem aReferer( SID_REFERER, OUString("private:user") );
                    SfxStringItem aFrame( SID_TARGETNAME, OUString("_blank") );

                    OUString aStrFlags('S');
                    if ( m_pRbtEditLater->IsChecked() )
                    {
                        aStrFlags += OUString('H');
                    }
                    SfxStringItem aFlags (SID_OPTIONS, aStrFlags);

                    // open url
                    const SfxPoolItem* pReturn = GetDispatcher()->Execute( SID_OPENDOC,
                                                                           SFX_CALLMODE_SYNCHRON,
                                                                           &aName, &aFlags,
                                                                           &aFrame, &aReferer, 0L );

                    // save new doc
                    const SfxViewFrameItem *pItem = PTR_CAST( SfxViewFrameItem, pReturn );  // SJ: pReturn is NULL if the Hyperlink
                    if ( pItem )                                                            // creation is cancelled #106216#
                    {
                        pViewFrame = pItem->GetFrame();
                        if (pViewFrame)
                        {
                            SfxStringItem aNewName( SID_FILE_NAME, aURL.GetMainURL( INetURLObject::NO_DECODE ) );

                            pViewFrame->GetDispatcher()->Execute( SID_SAVEASDOC,
                                                                  SFX_CALLMODE_SYNCHRON,
                                                                  &aNewName, 0L );

                        }
                    }
                }

                if ( m_pRbtEditNow->IsChecked() && pCurrentDocFrame )
                {
                    pCurrentDocFrame->ToTop();
                }
            }
        }
        catch (const uno::Exception&)
        {
        }

        if ( pViewFrame && m_pRbtEditLater->IsChecked() )
        {
            SfxObjectShell* pObjShell = pViewFrame->GetObjectShell();
            pObjShell->DoClose();
        }
    }

    LeaveWait();
}

/*************************************************************************
|*
|* Click on imagebutton : new
|*
|************************************************************************/

IMPL_LINK_NOARG(SvxHyperlinkNewDocTp, ClickNewHdl_Impl)
{
    uno::Reference < XComponentContext > xContext( ::comphelper::getProcessComponentContext() );
    uno::Reference < XFolderPicker2 >  xFolderPicker = FolderPicker::create(xContext);

    OUString            aStrURL;
    OUString            aTempStrURL( m_pCbbPath->GetText() );
    utl::LocalFileHelper::ConvertSystemPathToURL( aTempStrURL, m_pCbbPath->GetBaseURL(), aStrURL );

    OUString            aStrPath = aStrURL;
    bool            bZeroPath = aStrPath.isEmpty();
    bool            bHandleFileName = bZeroPath;    // when path has length of 0, then the rest should always be handled
                                                        //  as file name, otherwise we do not yet know

    if( bZeroPath )
        aStrPath = SvtPathOptions().GetWorkPath();
    else if( !::utl::UCBContentHelper::IsFolder( aStrURL ) )
        bHandleFileName = true;

    xFolderPicker->setDisplayDirectory( aStrPath );
    DisableClose( true );
    sal_Int16 nResult = xFolderPicker->execute();
    DisableClose( false );
    if( ExecutableDialogResults::OK == nResult )
    {
        sal_Char const  sSlash[] = "/";

        INetURLObject   aURL( aStrURL, INET_PROT_FILE );
        OUString        aStrName;
        if( bHandleFileName )
            aStrName = bZeroPath? aTempStrURL : OUString(aURL.getName());

        m_pCbbPath->SetBaseURL( xFolderPicker->getDirectory() );
        OUString          aStrTmp( xFolderPicker->getDirectory() );

        if( aStrTmp[ aStrTmp.getLength() - 1 ] != sSlash[0] )
            aStrTmp += OUString( sSlash );

        // append old file name
        if( bHandleFileName )
            aStrTmp += aStrName;

        INetURLObject   aNewURL( aStrTmp );

        if( !aStrName.isEmpty() && !aNewURL.getExtension().isEmpty() &&
            m_pLbDocTypes->GetSelectEntryPos() != LISTBOX_ENTRY_NOTFOUND )
        {
            // get private-url
            sal_uInt16 nPos = m_pLbDocTypes->GetSelectEntryPos();
            aNewURL.setExtension( ( ( DocumentTypeData* ) m_pLbDocTypes->GetEntryData( nPos ) )->aStrExt );
        }

        if( aNewURL.GetProtocol() == INET_PROT_FILE )
        {
            utl::LocalFileHelper::ConvertURLToSystemPath( aNewURL.GetMainURL( INetURLObject::NO_DECODE ), aStrTmp );
        }
        else
        {
            aStrTmp = aNewURL.GetMainURL( INetURLObject::DECODE_UNAMBIGUOUS );
        }

        m_pCbbPath->SetText ( aStrTmp );
    }
    return( 0L );
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
