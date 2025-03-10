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
#ifndef INCLUDED_SFX2_TABDLG_HXX
#define INCLUDED_SFX2_TABDLG_HXX

#include <sal/config.h>
#include <sfx2/dllapi.h>
#include <sal/types.h>
#include <vcl/button.hxx>
#include <vcl/layout.hxx>
#include <vcl/tabctrl.hxx>
#include <vcl/tabdlg.hxx>
#include <vcl/tabpage.hxx>
#include <svl/itempool.hxx>
#include <svl/itemset.hxx>
#include <com/sun/star/frame/XFrame.hpp>

class SfxPoolItem;
class SfxTabDialog;
class SfxViewFrame;
class SfxTabPage;
class SfxBindings;

typedef SfxTabPage* (*CreateTabPage)(Window *pParent, const SfxItemSet *rAttrSet);
typedef const sal_uInt16*     (*GetTabPageRanges)(); // provides international Which-value
struct TabPageImpl;

struct TabDlg_Impl;

#define ID_TABCONTROL   1
#define RET_USER        100
#define RET_USER_CANCEL 101

class SFX2_DLLPUBLIC SfxTabDialogItem: public SfxSetItem
{
public:
    TYPEINFO_OVERRIDE();
                            SfxTabDialogItem( sal_uInt16 nId, const SfxItemSet& rItemSet );
                            SfxTabDialogItem(const SfxTabDialogItem& rAttr, SfxItemPool* pItemPool=NULL);
    virtual SfxPoolItem*    Clone(SfxItemPool* pToPool) const SAL_OVERRIDE;
    virtual SfxPoolItem*    Create(SvStream& rStream, sal_uInt16 nVersion) const SAL_OVERRIDE;
};

class SFX2_DLLPUBLIC SfxTabDialog : public TabDialog
{
private:
friend class SfxTabPage;
friend class SfxTabDialogController;

    SfxViewFrame*   pFrame;

    VclBox *m_pBox;
    TabControl *m_pTabCtrl;

    PushButton* m_pOKBtn;
    PushButton* m_pApplyBtn;
    PushButton* m_pUserBtn;
    CancelButton* m_pCancelBtn;
    HelpButton* m_pHelpBtn;
    PushButton* m_pResetBtn;
    PushButton* m_pBaseFmtBtn;

    bool m_bOwnsVBox;
    bool m_bOwnsTabCtrl;
    bool m_bOwnsActionArea;
    bool m_bOwnsOKBtn;
    bool m_bOwnsApplyBtn;
    bool m_bOwnsUserBtn;
    bool m_bOwnsCancelBtn;
    bool m_bOwnsHelpBtn;
    bool m_bOwnsResetBtn;
    bool m_bOwnsBaseFmtBtn;

    const SfxItemSet*   pSet;
    SfxItemSet*         pOutSet;
    TabDlg_Impl*        pImpl;
    sal_uInt16*         pRanges;
    sal_uInt16          nAppPageId;
    bool                bItemsReset;
    bool                bStandardPushed;

    DECL_DLLPRIVATE_LINK( ActivatePageHdl, TabControl * );
    DECL_DLLPRIVATE_LINK( DeactivatePageHdl, TabControl * );
    DECL_DLLPRIVATE_LINK(OkHdl, void *);
    DECL_DLLPRIVATE_LINK(ResetHdl, void *);
    DECL_DLLPRIVATE_LINK(BaseFmtHdl, void *);
    DECL_DLLPRIVATE_LINK(UserHdl, void *);
    DECL_DLLPRIVATE_LINK(CancelHdl, void *);
    SAL_DLLPRIVATE void Init_Impl(bool bFmtFlag, const OUString* pUserButtonText, const ResId* pResId);

protected:
    virtual short               Ok();
    // Is deleted in Sfx!
    virtual SfxItemSet*         CreateInputItemSet( sal_uInt16 nId );
    // Is not deleted in Sfx!
    virtual const SfxItemSet*   GetRefreshedSet();
    virtual void                PageCreated( sal_uInt16 nId, SfxTabPage &rPage );

    VclButtonBox*   m_pActionArea;
    SfxItemSet*     pExampleSet;
    SfxItemSet*     GetInputSetImpl();
    SfxTabPage*     GetTabPage( sal_uInt16 nPageId ) const;

    /** prepare to leace the current page. Calls the DeactivatePage method of the current page, (if necessary),
        handles the item sets to copy.
        @return sal_True if it is allowed to leave the current page, sal_False otherwise
    */
    bool PrepareLeaveCurrentPage();

    /** save the position of the TabDialog and which tab page is the currently active one
     */
    void SavePosAndId();

public:
    SfxTabDialog(Window* pParent,
                 const OString& rID, const OUString& rUIXMLDescription,
                 const SfxItemSet * = 0, bool bEditFmt = false);
    SfxTabDialog(SfxViewFrame *pViewFrame, Window* pParent,
                 const OString& rID, const OUString& rUIXMLDescription,
                 const SfxItemSet * = 0, bool bEditFmt = false);
    virtual ~SfxTabDialog();

    sal_uInt16          AddTabPage( const OString& rName,           // Name of the label for the page in the notebook .ui
                                    CreateTabPage pCreateFunc,      // != 0
                                    GetTabPageRanges pRangesFunc,   // can be 0
                                    bool bItemsOnDemand = false);

    sal_uInt16          AddTabPage ( const OString &rName,          // Name of the label for the page in the notebook .ui
                                     sal_uInt16 nPageCreateId );    // Identifier of the Factory Method to create the page

    void                AddTabPage( sal_uInt16 nId,
                                    const OUString &rRiderText,
                                    CreateTabPage pCreateFunc,      // != 0
                                    GetTabPageRanges pRangesFunc,   // can be 0
                                    bool bItemsOnDemand = false,
                                    sal_uInt16 nPos = TAB_APPEND);
    void                AddTabPage( sal_uInt16 nId,
                                    const Bitmap &rRiderBitmap,
                                    CreateTabPage pCreateFunc,      // != 0
                                    GetTabPageRanges pRangesFunc,   // can be 0
                                    bool bItemsOnDemand = false,
                                    sal_uInt16 nPos = TAB_APPEND);

    void                AddTabPage( sal_uInt16 nId,
                                    const OUString &rRiderText,
                                    bool bItemsOnDemand = false,
                                    sal_uInt16 nPos = TAB_APPEND);
    void                AddTabPage( sal_uInt16 nId,
                                    const Bitmap &rRiderBitmap,
                                    bool bItemsOnDemand = false,
                                    sal_uInt16 nPos = TAB_APPEND);

    void                RemoveTabPage( const OString& rName ); // Name of the label for the page in the notebook .ui
    void                RemoveTabPage( sal_uInt16 nId );

    void                SetCurPageId(sal_uInt16 nId)
    {
        nAppPageId = nId;
    }
    void                SetCurPageId(const OString& rName)
    {
        nAppPageId = m_pTabCtrl->GetPageId(rName);
    }
    sal_uInt16          GetCurPageId() const
    {
        return m_pTabCtrl->GetCurPageId();
    }

    SfxTabPage* GetCurTabPage() const
    {
        return GetTabPage(m_pTabCtrl->GetCurPageId());
    }

    OUString            GetPageText( sal_uInt16 nPageId ) const
    {
        return m_pTabCtrl->GetPageText(nPageId);
    }

    void                ShowPage( sal_uInt16 nId );

    // may provide local slots converted by Map
    const sal_uInt16*       GetInputRanges( const SfxItemPool& );
    void                SetInputSet( const SfxItemSet* pInSet );
    const SfxItemSet*   GetOutputItemSet() const { return pOutSet; }

    const PushButton&   GetOKButton() const { return *m_pOKBtn; }
    PushButton&         GetOKButton() { return *m_pOKBtn; }
    const CancelButton& GetCancelButton() const { return *m_pCancelBtn; }
    CancelButton&       GetCancelButton() { return *m_pCancelBtn; }
    const HelpButton&   GetHelpButton() const { return *m_pHelpBtn; }
    HelpButton&         GetHelpButton() { return *m_pHelpBtn; }

    const PushButton*   GetUserButton() const { return m_pUserBtn; }
    PushButton*         GetUserButton() { return m_pUserBtn; }
    void                RemoveResetButton();
    void                RemoveStandardButton();

    short               Execute() SAL_OVERRIDE;
    void                StartExecuteModal( const Link& rEndDialogHdl ) SAL_OVERRIDE;
    void                Start( bool bShow = true );

    const SfxItemSet*   GetExampleSet() const { return pExampleSet; }
    SfxViewFrame*       GetViewFrame() const { return pFrame; }

    void                SetApplyHandler(const Link& _rHdl);

    SAL_DLLPRIVATE void Start_Impl();

    //calls Ok without closing dialog
    bool Apply();
};

namespace sfx { class ItemConnectionBase; }

class SFX2_DLLPUBLIC SfxTabPage: public TabPage
{
friend class SfxTabDialog;

private:
    const SfxItemSet*   pSet;
    OUString            aUserString;
    bool                bHasExchangeSupport;
    TabPageImpl*        pImpl;

    SAL_DLLPRIVATE void SetInputSet( const SfxItemSet* pNew ) { pSet = pNew; }

protected:
    SfxTabPage( Window *pParent, const ResId &, const SfxItemSet &rAttrSet );
    SfxTabPage(Window *pParent, const OString& rID, const OUString& rUIXMLDescription, const SfxItemSet *rAttrSet);

    sal_uInt16              GetSlot( sal_uInt16 nWhich ) const
                            { return pSet->GetPool()->GetSlotId( nWhich ); }
    sal_uInt16              GetWhich( sal_uInt16 nSlot, bool bDeep = true ) const
                            { return pSet->GetPool()->GetWhich( nSlot, bDeep ); }
    const SfxPoolItem*  GetOldItem( const SfxItemSet& rSet, sal_uInt16 nSlot, bool bDeep = true );
    SfxTabDialog*       GetTabDialog() const;

    void                AddItemConnection( sfx::ItemConnectionBase* pConnection );

public:
    virtual             ~SfxTabPage();

    const SfxItemSet&   GetItemSet() const { return *pSet; }

    virtual bool        FillItemSet( SfxItemSet* );
    virtual void        Reset( const SfxItemSet* );

    bool                HasExchangeSupport() const
                            { return bHasExchangeSupport; }
    void                SetExchangeSupport( bool bNew = true )
                            { bHasExchangeSupport = bNew; }

    enum sfxpg {
      KEEP_PAGE = 0x0000,      // Error handling; page does not change
        // 2. Fill an itemset for update
        // parent examples, this pointer can be NULL all the time!
        LEAVE_PAGE = 0x0001,
        // Set, refresh and update other Page
        REFRESH_SET = 0x0002
    };

        using TabPage::ActivatePage;
        using TabPage::DeactivatePage;
    virtual void            ActivatePage( const SfxItemSet& );
    virtual int             DeactivatePage( SfxItemSet* pSet = 0 );
    void                    SetUserData(const OUString& rString)
                              { aUserString = rString; }
    OUString                GetUserData() { return aUserString; }
    virtual void            FillUserData();
    virtual bool            IsReadOnly() const;
    virtual void PageCreated (const SfxAllItemSet& aSet);
    static const SfxPoolItem* GetItem( const SfxItemSet& rSet, sal_uInt16 nSlot, bool bDeep = true );

    void SetFrame(const ::com::sun::star::uno::Reference< ::com::sun::star::frame::XFrame >& xFrame);
    ::com::sun::star::uno::Reference< ::com::sun::star::frame::XFrame > GetFrame();
};

#endif

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
