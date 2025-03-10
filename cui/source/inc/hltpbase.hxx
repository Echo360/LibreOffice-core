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
#ifndef INCLUDED_CUI_SOURCE_INC_HLTPBASE_HXX
#define INCLUDED_CUI_SOURCE_INC_HLTPBASE_HXX

#include <sfx2/app.hxx>
#include <sfx2/tabdlg.hxx>
#include <vcl/group.hxx>
#include <vcl/button.hxx>
#include <vcl/fixed.hxx>
#include <vcl/combobox.hxx>
#include <vcl/edit.hxx>
#include <vcl/lstbox.hxx>
#include <svl/stritem.hxx>
#include <svl/eitem.hxx>
#include <svtools/transfer.hxx>
#include <sfx2/dispatch.hxx>
#include <vcl/msgbox.hxx>
#include <sfx2/fcontnr.hxx>
#include <svtools/inettbc.hxx>
#include <vcl/timer.hxx>

#include <dialmgr.hxx>
#include <sfx2/docfile.hxx>
#include <cuires.hrc>
#include <com/sun/star/frame/XFrame.hpp>
#include "helpid.hrc"
#include <svx/hlnkitem.hxx>

#include "hlmarkwn.hxx"
#include "iconcdlg.hxx"

/// ComboBox-Control for URL's with History and Autocompletion
class SvxHyperURLBox : public SvtURLBox, public DropTargetHelper
{
protected:
    virtual sal_Int8    AcceptDrop( const AcceptDropEvent& rEvt ) SAL_OVERRIDE;
    virtual sal_Int8    ExecuteDrop( const ExecuteDropEvent& rEvt ) SAL_OVERRIDE;

    virtual bool        Notify( NotifyEvent& rNEvt ) SAL_OVERRIDE;
    virtual void        Select() SAL_OVERRIDE;
    virtual void        Modify() SAL_OVERRIDE;
    virtual bool        PreNotify( NotifyEvent& rNEvt ) SAL_OVERRIDE;

public:
    SvxHyperURLBox( Window* pParent, INetProtocol eSmart = INET_PROT_FILE );

};

/// Tabpage : Basisclass
class SvxHyperlinkTabPageBase : public IconChoicePage
{
private:
    ComboBox            *mpCbbFrame;
    ListBox             *mpLbForm;
    Edit                *mpEdIndication;
    Edit                *mpEdText;
    PushButton          *mpBtScript;

    bool            mbIsCloseDisabled;

    ::com::sun::star::uno::Reference< ::com::sun::star::frame::XFrame >
                        mxDocumentFrame;

protected:
    Window*             mpDialog;

    bool                mbStdControlsInit;

    OUString            maStrInitURL;

    Timer               maTimer;

    SvxHlinkDlgMarkWnd* mpMarkWnd;

    void InitStdControls ();
    virtual void FillStandardDlgFields ( SvxHyperlinkItem* pHyperlinkItem );
    virtual void FillDlgFields(const OUString& rStrURL) = 0;
    virtual void GetCurentItemData     ( OUString& rStrURL, OUString& aStrName,
                                         OUString& aStrIntName, OUString& aStrFrame,
                                         SvxLinkInsertMode& eMode ) = 0;
    virtual OUString CreateUiNameFromURL( const OUString& aStrURL );

    void         GetDataFromCommonFields( OUString& aStrName,
                                          OUString& aStrIntName, OUString& aStrFrame,
                                          SvxLinkInsertMode& eMode );

    DECL_LINK (ClickScriptHdl_Impl, void * ); ///< Button : Script

    OUString            aEmptyStr;

    static OUString GetSchemeFromURL( const OUString& rStrURL );

    inline void     DisableClose( bool _bDisable ) { mbIsCloseDisabled = _bDisable; }

public:
    SvxHyperlinkTabPageBase (
        Window *pParent,
        IconChoiceDialog* pDlg,
        const OString& rID,
        const OUString& rUIXMLDescription,
        const SfxItemSet& rItemSet
    );
    virtual ~SvxHyperlinkTabPageBase ();

    void    SetDocumentFrame(
        const ::com::sun::star::uno::Reference< ::com::sun::star::frame::XFrame >& rxDocumentFrame )
    {
        mxDocumentFrame = rxDocumentFrame;
    }

    virtual bool AskApply ();
    virtual void DoApply ();
    virtual void SetInitFocus();
    virtual void SetMarkStr ( const OUString& aStrMark );
    virtual void Reset( const SfxItemSet& ) SAL_OVERRIDE;
    virtual bool FillItemSet( SfxItemSet* ) SAL_OVERRIDE;
    virtual void ActivatePage( const SfxItemSet& rItemSet ) SAL_OVERRIDE;
    virtual int  DeactivatePage( SfxItemSet* pSet = 0 ) SAL_OVERRIDE;

    bool IsMarkWndVisible ()      { return ((Window*)mpMarkWnd)->IsVisible(); }
    Size GetSizeExtraWnd ()       { return ( mpMarkWnd->GetSizePixel() ); }
    bool MoveToExtraWnd ( Point aNewPos, bool bDisConnectDlg = false );

    virtual void        ActivatePage() SAL_OVERRIDE;
    virtual void        DeactivatePage() SAL_OVERRIDE;
    virtual bool        QueryClose() SAL_OVERRIDE;

protected:
    virtual bool ShouldOpenMarkWnd();
    virtual void SetMarkWndShouldOpen(sal_Bool bOpen);

    void ShowMarkWnd ();
    void HideMarkWnd ()           { ( ( Window* ) mpMarkWnd )->Hide(); }
    void InvalidateMarkWnd ()     { ( ( Window* ) mpMarkWnd )->Invalidate(); }

    SfxDispatcher* GetDispatcher() const;

    sal_uInt16             GetMacroEvents();
    SvxMacroTableDtor* GetMacroTable();

    bool IsHTMLDoc() const;
};

#endif // INCLUDED_CUI_SOURCE_INC_HLTPBASE_HXX

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
