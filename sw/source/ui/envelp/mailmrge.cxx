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

#include <vcl/msgbox.hxx>
#include <vcl/svapp.hxx>
#include <vcl/settings.hxx>

#include <tools/urlobj.hxx>
#include <svl/urihelper.hxx>
#include <unotools/pathoptions.hxx>
#include <svl/mailenum.hxx>
#include <svx/svxdlg.hxx>
#include <svx/dialogs.hrc>
#include <helpid.h>
#include <view.hxx>
#include <docsh.hxx>
#include <IDocumentDeviceAccess.hxx>
#include <wrtsh.hxx>
#include <dbmgr.hxx>
#include <dbui.hxx>
#include <prtopt.hxx>
#include <swmodule.hxx>
#include <modcfg.hxx>
#include <mailmergehelper.hxx>
#include <envelp.hrc>
#include <mailmrge.hxx>
#include <sfx2/docfile.hxx>
#include <sfx2/docfilt.hxx>
#include <comphelper/sequenceashashmap.hxx>
#include <com/sun/star/frame/XDispatchProvider.hpp>
#include <com/sun/star/container/XChild.hpp>
#include <com/sun/star/container/XContainerQuery.hpp>
#include <com/sun/star/container/XEnumeration.hpp>
#include <com/sun/star/form/runtime/XFormController.hpp>
#include <com/sun/star/frame/Frame.hpp>
#include <com/sun/star/sdbcx/XColumnsSupplier.hpp>
#include <com/sun/star/sdbcx/XRowLocate.hpp>
#include <com/sun/star/sdb/XResultSetAccess.hpp>
#include <com/sun/star/sdbc/XDataSource.hpp>
#include <com/sun/star/ui/dialogs/FolderPicker.hpp>
#include <toolkit/helper/vclunohelper.hxx>
#include <comphelper/processfactory.hxx>
#include <cppuhelper/implbase1.hxx>

#include <unomid.h>

#include <algorithm>

using namespace ::com::sun::star;
using namespace ::com::sun::star::container;
using namespace ::com::sun::star::lang;
using namespace ::com::sun::star::sdb;
using namespace ::com::sun::star::sdbc;
using namespace ::com::sun::star::sdbcx;
using namespace ::com::sun::star::beans;
using namespace ::com::sun::star::util;
using namespace ::com::sun::star::uno;
using namespace ::com::sun::star::frame;
using namespace ::com::sun::star::form;
using namespace ::com::sun::star::view;
using namespace ::com::sun::star::ui::dialogs;

struct SwMailMergeDlg_Impl
{
    uno::Reference<runtime::XFormController> xFController;
    uno::Reference<XSelectionChangeListener> xChgLstnr;
    uno::Reference<XSelectionSupplier> xSelSupp;
};

class SwXSelChgLstnr_Impl : public cppu::WeakImplHelper1
<
    view::XSelectionChangeListener
>
{
    SwMailMergeDlg& rParent;
public:
    SwXSelChgLstnr_Impl(SwMailMergeDlg& rParentDlg);
    virtual ~SwXSelChgLstnr_Impl();

    virtual void SAL_CALL selectionChanged( const EventObject& aEvent ) throw (RuntimeException, std::exception) SAL_OVERRIDE;
    virtual void SAL_CALL disposing( const EventObject& Source ) throw (RuntimeException, std::exception) SAL_OVERRIDE;
};

SwXSelChgLstnr_Impl::SwXSelChgLstnr_Impl(SwMailMergeDlg& rParentDlg) :
    rParent(rParentDlg)
{}

SwXSelChgLstnr_Impl::~SwXSelChgLstnr_Impl()
{}

void SwXSelChgLstnr_Impl::selectionChanged( const EventObject&  ) throw (RuntimeException, std::exception)
{
    //call the parent to enable selection mode
    Sequence <Any> aSelection;
    if(rParent.pImpl->xSelSupp.is())
        rParent.pImpl->xSelSupp->getSelection() >>= aSelection;

    bool bEnable = aSelection.getLength() > 0;
    rParent.m_pMarkedRB->Enable(bEnable);
    if(bEnable)
        rParent.m_pMarkedRB->Check();
    else if(rParent.m_pMarkedRB->IsChecked())
    {
        rParent.m_pAllRB->Check();
        rParent.m_aSelection.realloc(0);
    }
}

void SwXSelChgLstnr_Impl::disposing( const EventObject&  ) throw (RuntimeException, std::exception)
{
    OSL_FAIL("disposing");
}

SwMailMergeDlg::SwMailMergeDlg(Window* pParent, SwWrtShell& rShell,
        const OUString& rSourceName,
        const OUString& rTblName,
        sal_Int32 nCommandType,
        const uno::Reference< XConnection>& _xConnection,
        Sequence< Any >* pSelection) :

    SvxStandardDialog(pParent, "MailmergeDialog", "modules/swriter/ui/mailmerge.ui"),

    pImpl           (new SwMailMergeDlg_Impl),

    rSh             (rShell),
    nMergeType      (DBMGR_MERGE_MAILING),
    m_aDialogSize( GetSizePixel() )
{
    get(m_pBeamerWin, "beamer");

    get(m_pAllRB, "all");
    get(m_pMarkedRB, "selected");
    get(m_pFromRB, "rbfrom");
    get(m_pFromNF, "from");
    get(m_pToNF, "to");

    get(m_pPrinterRB, "printer");
    get(m_pMailingRB, "electronic");
    get(m_pFileRB, "file");

    get(m_pSingleJobsCB, "singlejobs");

    get(m_pSaveMergedDocumentFT, "savemergeddoclabel");
    get(m_pSaveSingleDocRB, "singledocument");
    get(m_pSaveIndividualRB, "idividualdocuments");
    get(m_pGenerateFromDataBaseCB, "generate");

    get(m_pColumnFT, "fieldlabel");
    get(m_pColumnLB, "field");

    get(m_pPathFT, "pathlabel");
    get(m_pPathED, "path");
    get(m_pPathPB, "pathpb");
    get(m_pFilterFT, "fileformatlabel");
    get(m_pFilterLB, "fileformat");

    get(m_pAddressFldLB, "address");
    get(m_pSubjectFT, "subjectlabel");
    get(m_pSubjectED, "subject");
    get(m_pFormatFT, "mailformatlabel");
    get(m_pAttachFT, "attachmentslabel");
    get(m_pAttachED, "attachments");
    get(m_pAttachPB, "attach");
    get(m_pFormatHtmlCB, "html");
    get(m_pFormatRtfCB, "rtf");
    get(m_pFormatSwCB, "swriter");

    get(m_pOkBTN, "ok");

    m_pSingleJobsCB->Show(false); // not supported in since cws printerpullpages anymore
    //task #97066# mailing of form letters is currently not supported
    m_pMailingRB->Show(false);
    m_pSubjectFT->Show(false);
    m_pSubjectED->Show(false);
    m_pFormatFT->Show(false);
    m_pFormatSwCB->Show(false);
    m_pFormatHtmlCB->Show(false);
    m_pFormatRtfCB->Show(false);
    m_pAttachFT->Show(false);
    m_pAttachED->Show(false);
    m_pAttachPB->Show(false);

    Point aMailPos = m_pMailingRB->GetPosPixel();
    Point aFilePos = m_pFileRB->GetPosPixel();
    aFilePos.X() -= (aFilePos.X() - aMailPos.X()) /2;
    m_pFileRB->SetPosPixel(aFilePos);
    uno::Reference< lang::XMultiServiceFactory > xMSF = comphelper::getProcessServiceFactory();
    if(pSelection)
    {
        m_aSelection = *pSelection;
        m_pBeamerWin->Show(false);
    }
    else
    {
        try
        {
            // create a frame wrapper for myself
            m_xFrame = frame::Frame::create( comphelper::getProcessComponentContext() );
            m_pUIBuilder->drop_ownership(m_pBeamerWin);
            m_xFrame->initialize( VCLUnoHelper::GetInterface ( m_pBeamerWin ) );
        }
        catch (const Exception&)
        {
            m_xFrame.clear();
        }
        if(m_xFrame.is())
        {
            URL aURL;
            aURL.Complete = ".component:DB/DataSourceBrowser";
            uno::Reference<XDispatch> xD = m_xFrame->queryDispatch(aURL,
                        "",
                        0x0C);
            if(xD.is())
            {
                Sequence<PropertyValue> aProperties(3);
                PropertyValue* pProperties = aProperties.getArray();
                pProperties[0].Name = "DataSourceName";
                pProperties[0].Value <<= rSourceName;
                pProperties[1].Name = "Command";
                pProperties[1].Value <<= rTblName;
                pProperties[2].Name = "CommandType";
                pProperties[2].Value <<= nCommandType;
                xD->dispatch(aURL, aProperties);
                m_pBeamerWin->Show();
            }
            uno::Reference<XController> xController = m_xFrame->getController();
            pImpl->xFController = uno::Reference<runtime::XFormController>(xController, UNO_QUERY);
            if(pImpl->xFController.is())
            {
                uno::Reference< awt::XControl > xCtrl = pImpl->xFController->getCurrentControl(  );
                pImpl->xSelSupp = uno::Reference<XSelectionSupplier>(xCtrl, UNO_QUERY);
                if(pImpl->xSelSupp.is())
                {
                    pImpl->xChgLstnr = new SwXSelChgLstnr_Impl(*this);
                    pImpl->xSelSupp->addSelectionChangeListener(  pImpl->xChgLstnr );
                }
            }
        }
    }

    pModOpt = SW_MOD()->GetModuleConfig();

    sal_Int16 nMailingMode(pModOpt->GetMailingFormats());
    m_pFormatSwCB->Check((nMailingMode & TXTFORMAT_OFFICE) != 0);
    m_pFormatHtmlCB->Check((nMailingMode & TXTFORMAT_HTML) != 0);
    m_pFormatRtfCB->Check((nMailingMode & TXTFORMAT_RTF) != 0);

    m_pAllRB->Check(true);

    // Install handlers
    Link aLk = LINK(this, SwMailMergeDlg, ButtonHdl);
    m_pOkBTN->SetClickHdl(aLk);

    m_pPathPB->SetClickHdl(LINK(this, SwMailMergeDlg, InsertPathHdl));

    aLk = LINK(this, SwMailMergeDlg, OutputTypeHdl);
    m_pPrinterRB->SetClickHdl(aLk);
    m_pFileRB->SetClickHdl(aLk);

    //#i63267# printing might be disabled
    bool bIsPrintable = !Application::GetSettings().GetMiscSettings().GetDisablePrinting();
    m_pPrinterRB->Enable(bIsPrintable);
    OutputTypeHdl(bIsPrintable ? m_pPrinterRB : m_pFileRB);

    aLk = LINK(this, SwMailMergeDlg, FilenameHdl);
    m_pGenerateFromDataBaseCB->SetClickHdl( aLk );
    bool bColumn = pModOpt->IsNameFromColumn();
    if(bColumn)
        m_pGenerateFromDataBaseCB->Check();

    FilenameHdl( m_pGenerateFromDataBaseCB );
    aLk = LINK(this, SwMailMergeDlg, SaveTypeHdl);
    m_pSaveSingleDocRB->Check( true );
    m_pSaveSingleDocRB->SetClickHdl( aLk );
    m_pSaveIndividualRB->SetClickHdl( aLk );
    aLk.Call( m_pSaveSingleDocRB );

    aLk = LINK(this, SwMailMergeDlg, ModifyHdl);
    m_pFromNF->SetModifyHdl(aLk);
    m_pToNF->SetModifyHdl(aLk);
    m_pFromNF->SetMax(SAL_MAX_INT32);
    m_pToNF->SetMax(SAL_MAX_INT32);

    SwDBManager* pDBManager = rSh.GetDBManager();
    if(_xConnection.is())
        pDBManager->GetColumnNames(m_pAddressFldLB, _xConnection, rTblName);
    else
        pDBManager->GetColumnNames(m_pAddressFldLB, rSourceName, rTblName);
    for(sal_Int32 nEntry = 0; nEntry < m_pAddressFldLB->GetEntryCount(); ++nEntry)
        m_pColumnLB->InsertEntry(m_pAddressFldLB->GetEntry(nEntry));

    m_pAddressFldLB->SelectEntry("EMAIL");

    OUString sPath(pModOpt->GetMailingPath());
    if(sPath.isEmpty())
    {
        SvtPathOptions aPathOpt;
        sPath = aPathOpt.GetWorkPath();
    }
    INetURLObject aURL(sPath);
    if(aURL.GetProtocol() == INET_PROT_FILE)
        m_pPathED->SetText(aURL.PathToFileName());
    else
        m_pPathED->SetText(aURL.GetFull());

    if (!bColumn )
    {
        m_pColumnLB->SelectEntry("NAME");
    }
    else
        m_pColumnLB->SelectEntry(pModOpt->GetNameFromColumn());

    if (m_pAddressFldLB->GetSelectEntryCount() == 0)
        m_pAddressFldLB->SelectEntryPos(0);
    if (m_pColumnLB->GetSelectEntryCount() == 0)
        m_pColumnLB->SelectEntryPos(0);

    const bool bEnable = m_aSelection.getLength() != 0;
    m_pMarkedRB->Enable(bEnable);
    if (bEnable)
        m_pMarkedRB->Check();
    else
    {
        m_pAllRB->Check();
        m_pMarkedRB->Enable(false);
    }
    SetMinOutputSizePixel(m_aDialogSize);
    try
    {
        uno::Reference< container::XNameContainer> xFilterFactory(
                xMSF->createInstance("com.sun.star.document.FilterFactory"), UNO_QUERY_THROW);
        uno::Reference< container::XContainerQuery > xQuery(xFilterFactory, UNO_QUERY_THROW);
        const OUString sCommand("matchByDocumentService=com.sun.star.text.TextDocument:iflags="
            + OUString::number(SFX_FILTER_EXPORT)
            + ":eflags="
            + OUString::number(SFX_FILTER_NOTINFILEDLG)
            + ":default_first");
        uno::Reference< container::XEnumeration > xList = xQuery->createSubSetEnumerationByQuery(sCommand);
        const OUString sName("Name");
        sal_Int32 nODT = -1;
        while(xList->hasMoreElements())
        {
            comphelper::SequenceAsHashMap aFilter(xList->nextElement());
            const OUString sFilter = aFilter.getUnpackedValueOrDefault(sName, OUString());

            uno::Any aProps = xFilterFactory->getByName(sFilter);
            uno::Sequence< beans::PropertyValue > aFilterProperties;
            aProps >>= aFilterProperties;
            OUString sUIName2;
            const beans::PropertyValue* pFilterProperties = aFilterProperties.getConstArray();
            for(sal_Int32 nProp = 0; nProp < aFilterProperties.getLength(); ++nProp)
            {
                if(pFilterProperties[nProp].Name == "UIName")
                {
                    pFilterProperties[nProp].Value >>= sUIName2;
                    break;
                }
            }
            if( !sUIName2.isEmpty() )
            {
                const sal_Int32 nFilter = m_pFilterLB->InsertEntry( sUIName2 );
                if( sFilter.equalsAscii("writer8") )
                    nODT = nFilter;
                m_pFilterLB->SetEntryData( nFilter, new OUString( sFilter ) );
            }
        }
        m_pFilterLB->SelectEntryPos( nODT );
    }
    catch (const uno::Exception&)
    {
    }
}

SwMailMergeDlg::~SwMailMergeDlg()
{
    if(m_xFrame.is())
    {
        m_xFrame->setComponent(NULL, NULL);
        m_xFrame->dispose();
    }

    for( sal_Int32 nFilter = 0; nFilter < m_pFilterLB->GetEntryCount(); ++nFilter )
    {
        OUString* pData = reinterpret_cast< OUString* >( m_pFilterLB->GetEntryData(nFilter) );
        delete pData;
    }
    delete pImpl;
}

void SwMailMergeDlg::Apply()
{
}

IMPL_LINK( SwMailMergeDlg, ButtonHdl, Button *, pBtn )
{
    if (pBtn == m_pOkBTN)
    {
        if( ExecQryShell() )
            EndDialog(RET_OK);
    }
    return 0;
}

IMPL_LINK( SwMailMergeDlg, OutputTypeHdl, RadioButton *, pBtn )
{
    bool bPrint = pBtn == m_pPrinterRB;
    m_pSingleJobsCB->Enable(bPrint);

    m_pSaveMergedDocumentFT->Enable( !bPrint );
    m_pSaveSingleDocRB->Enable( !bPrint );
    m_pSaveIndividualRB->Enable( !bPrint );

    if( !bPrint )
    {
        SaveTypeHdl( m_pSaveSingleDocRB->IsChecked() ? m_pSaveSingleDocRB : m_pSaveIndividualRB );
    }
    else
    {
        m_pPathFT->Enable(false);
        m_pPathED->Enable(false);
        m_pPathPB->Enable(false);
        m_pColumnFT->Enable(false);
        m_pColumnLB->Enable(false);
        m_pFilterFT->Enable(false);
        m_pFilterLB->Enable(false);
        m_pGenerateFromDataBaseCB->Enable(false);
    }

    return 0;
}

IMPL_LINK( SwMailMergeDlg, SaveTypeHdl, RadioButton*,  pBtn )
{
    bool bIndividual = pBtn == m_pSaveIndividualRB;

    m_pGenerateFromDataBaseCB->Enable( bIndividual );
    if( bIndividual )
    {
        FilenameHdl( m_pGenerateFromDataBaseCB );
    }
    else
    {
        m_pColumnFT->Enable(false);
        m_pColumnLB->Enable(false);
        m_pPathFT->Enable( false );
        m_pPathED->Enable( false );
        m_pPathPB->Enable( false );
        m_pFilterFT->Enable( false );
        m_pFilterLB->Enable( false );
    }
    return 0;
}

IMPL_LINK( SwMailMergeDlg, FilenameHdl, CheckBox*, pBox )
{
    bool bEnable = pBox->IsChecked();
    m_pColumnFT->Enable( bEnable );
    m_pColumnLB->Enable(bEnable);
    m_pPathFT->Enable( bEnable );
    m_pPathED->Enable(bEnable);
    m_pPathPB->Enable( bEnable );
    m_pFilterFT->Enable( bEnable );
    m_pFilterLB->Enable( bEnable );
 return 0;
}

IMPL_LINK_NOARG(SwMailMergeDlg, ModifyHdl)
{
    m_pFromRB->Check();
    return (0);
}

bool SwMailMergeDlg::ExecQryShell()
{
    if(pImpl->xSelSupp.is())
    {
        pImpl->xSelSupp->removeSelectionChangeListener(  pImpl->xChgLstnr );
    }
    SwDBManager* pMgr = rSh.GetDBManager();

    if (m_pPrinterRB->IsChecked())
        nMergeType = DBMGR_MERGE_MAILMERGE;
    else
    {
        nMergeType = static_cast< sal_uInt16 >( m_pSaveSingleDocRB->IsChecked() ?
                    DBMGR_MERGE_SINGLE_FILE : DBMGR_MERGE_MAILFILES );
        SfxMedium* pMedium = rSh.GetView().GetDocShell()->GetMedium();
        INetURLObject aAbs;
        if( pMedium )
            aAbs = pMedium->GetURLObject();
        OUString sPath(
            URIHelper::SmartRel2Abs(
                aAbs, m_pPathED->GetText(), URIHelper::GetMaybeFileHdl()));
        pModOpt->SetMailingPath(sPath);

        if (!sPath.endsWith("/"))
            sPath += "/";

        pModOpt->SetIsNameFromColumn(m_pGenerateFromDataBaseCB->IsChecked());

        if (m_pGenerateFromDataBaseCB->IsEnabled() && m_pGenerateFromDataBaseCB->IsChecked())
        {
            pMgr->SetEMailColumn(m_pColumnLB->GetSelectEntry());
            pModOpt->SetNameFromColumn(m_pColumnLB->GetSelectEntry());
            if( m_pFilterLB->GetSelectEntryPos() != LISTBOX_ENTRY_NOTFOUND)
                m_sSaveFilter = *static_cast<const OUString*>(m_pFilterLB->GetEntryData( m_pFilterLB->GetSelectEntryPos() ));
        }
        else
        {
            //#i97667# reset column name - otherwise it's remembered from the last run
            pMgr->SetEMailColumn(OUString());
            //start save as dialog
            OUString sFilter;
            sPath = SwMailMergeHelper::CallSaveAsDialog(sFilter);
            if (sPath.isEmpty())
                return false;
            m_sSaveFilter = sFilter;
        }

        pMgr->SetSubject(sPath);
    }

    if (m_pFromRB->IsChecked())    // Insert list
    {
        // Safe: the maximal value of the fields is limited
        sal_Int32 nStart = sal::static_int_cast<sal_Int32>(m_pFromNF->GetValue());
        sal_Int32 nEnd = sal::static_int_cast<sal_Int32>(m_pToNF->GetValue());

        if (nEnd < nStart)
            std::swap(nEnd, nStart);

        m_aSelection.realloc(nEnd - nStart + 1);
        Any* pSelection = m_aSelection.getArray();
        for (sal_Int32 i = nStart; i <= nEnd; ++i, ++pSelection)
            *pSelection <<= i;
    }
    else if (m_pAllRB->IsChecked() )
        m_aSelection.realloc(0);    // Empty selection = insert all
    else
    {
        if(pImpl->xSelSupp.is())
        {
            //update selection
            uno::Reference< XRowLocate > xRowLocate(GetResultSet(),UNO_QUERY);
            uno::Reference< XResultSet > xRes(xRowLocate,UNO_QUERY);
            pImpl->xSelSupp->getSelection() >>= m_aSelection;
            if ( xRowLocate.is() )
            {
                Any* pBegin = m_aSelection.getArray();
                Any* pEnd   = pBegin + m_aSelection.getLength();
                for (;pBegin != pEnd ; ++pBegin)
                {
                    if ( xRowLocate->moveToBookmark(*pBegin) )
                        *pBegin <<= xRes->getRow();
                }
            }
        }
    }
    IDocumentDeviceAccess* pIDDA = rSh.getIDocumentDeviceAccess();
    SwPrintData aPrtData( pIDDA->getPrintData() );
    aPrtData.SetPrintSingleJobs(m_pSingleJobsCB->IsChecked());
    pIDDA->setPrintData(aPrtData);

    pModOpt->SetSinglePrintJob(m_pSingleJobsCB->IsChecked());

    sal_uInt8 nMailingMode = 0;

    if (m_pFormatSwCB->IsChecked())
        nMailingMode |= TXTFORMAT_OFFICE;
    if (m_pFormatHtmlCB->IsChecked())
        nMailingMode |= TXTFORMAT_HTML;
    if (m_pFormatRtfCB->IsChecked())
        nMailingMode |= TXTFORMAT_RTF;
    pModOpt->SetMailingFormats(nMailingMode);
    return true;
}

IMPL_LINK_NOARG(SwMailMergeDlg, InsertPathHdl)
{
    OUString sPath( m_pPathED->GetText() );
    if( sPath.isEmpty() )
    {
        SvtPathOptions aPathOpt;
        sPath = aPathOpt.GetWorkPath();
    }

    uno::Reference< XComponentContext > xContext( ::comphelper::getProcessComponentContext() );
    uno::Reference < XFolderPicker2 > xFP = FolderPicker::create(xContext);
    xFP->setDisplayDirectory(sPath);
    if( xFP->execute() == RET_OK )
    {
        INetURLObject aURL(xFP->getDirectory());
        if(aURL.GetProtocol() == INET_PROT_FILE)
            m_pPathED->SetText(aURL.PathToFileName());
        else
            m_pPathED->SetText(aURL.GetFull());
    }
    return 0;
}

uno::Reference<XResultSet> SwMailMergeDlg::GetResultSet() const
{
    uno::Reference< XResultSet >  xResSetClone;
    if ( pImpl->xFController.is() )
    {
        // we create a clone to do the task
        uno::Reference< XResultSetAccess > xResultSetAccess( pImpl->xFController->getModel(),UNO_QUERY);
        if ( xResultSetAccess.is() )
            xResSetClone = xResultSetAccess->createResultSet();
    }
    return xResSetClone;
}

SwMailMergeCreateFromDlg::SwMailMergeCreateFromDlg(Window* pParent)
    : ModalDialog(pParent, "MailMergeDialog",
        "modules/swriter/ui/mailmergedialog.ui")
{
    get(m_pThisDocRB, "document");
}

SwMailMergeFieldConnectionsDlg::SwMailMergeFieldConnectionsDlg(Window* pParent)
    : ModalDialog(pParent, "MergeConnectDialog",
        "modules/swriter/ui/mergeconnectdialog.ui")
{
    get(m_pUseExistingRB, "existing");
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
