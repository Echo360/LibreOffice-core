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

#ifndef INCLUDED_SVX_SOURCE_INC_DOCRECOVERY_HXX
#define INCLUDED_SVX_SOURCE_INC_DOCRECOVERY_HXX

#include <vcl/dialog.hxx>
#include <vcl/button.hxx>
#include <vcl/fixed.hxx>
#include <vcl/lstbox.hxx>
#include <vcl/tabdlg.hxx>
#include <vcl/tabpage.hxx>
#include <svtools/simptabl.hxx>
#include <svtools/svlbitm.hxx>
#include <svtools/svmedit2.hxx>
#include <svtools/treelistbox.hxx>

#include <cppuhelper/implbase1.hxx>
#include <cppuhelper/implbase2.hxx>
#include <com/sun/star/task/StatusIndicatorFactory.hpp>
#include <com/sun/star/frame/XStatusListener.hpp>
#include <com/sun/star/frame/XDispatch.hpp>
#include <com/sun/star/lang/XComponent.hpp>


#define RECOVERY_CMDPART_PROTOCOL                   OUString( "vnd.sun.star.autorecovery:")

#define RECOVERY_CMDPART_DO_EMERGENCY_SAVE          OUString( "/doEmergencySave"         )
#define RECOVERY_CMDPART_DO_RECOVERY                OUString( "/doAutoRecovery"          )

#define RECOVERY_CMD_DO_PREPARE_EMERGENCY_SAVE      OUString( "vnd.sun.star.autorecovery:/doPrepareEmergencySave")
#define RECOVERY_CMD_DO_EMERGENCY_SAVE              OUString( "vnd.sun.star.autorecovery:/doEmergencySave"       )
#define RECOVERY_CMD_DO_RECOVERY                    OUString( "vnd.sun.star.autorecovery:/doAutoRecovery"        )
#define RECOVERY_CMD_DO_ENTRY_BACKUP                OUString( "vnd.sun.star.autorecovery:/doEntryBackup"         )
#define RECOVERY_CMD_DO_ENTRY_CLEANUP               OUString( "vnd.sun.star.autorecovery:/doEntryCleanUp"        )

#define PROP_STATUSINDICATOR                        OUString( "StatusIndicator"  )
#define PROP_DISPATCHASYNCHRON                      OUString( "DispatchAsynchron")
#define PROP_SAVEPATH                               OUString( "SavePath"         )
#define PROP_ENTRYID                                OUString( "EntryID"          )

#define STATEPROP_ID                                OUString( "ID"           )
#define STATEPROP_STATE                             OUString( "DocumentState")
#define STATEPROP_ORGURL                            OUString( "OriginalURL"  )
#define STATEPROP_TEMPURL                           OUString( "TempURL"      )
#define STATEPROP_FACTORYURL                        OUString( "FactoryURL"   )
#define STATEPROP_TEMPLATEURL                       OUString( "TemplateURL"  )
#define STATEPROP_TITLE                             OUString( "Title"        )
#define STATEPROP_MODULE                            OUString( "Module"       )

#define RECOVERY_OPERATIONSTATE_START               OUString( "start" )
#define RECOVERY_OPERATIONSTATE_STOP                OUString( "stop"  )
#define RECOVERY_OPERATIONSTATE_UPDATE              OUString( "update")

#define DLG_RET_UNKNOWN                                  -1
#define DLG_RET_OK                                        1
#define DLG_RET_CANCEL                                    0
#define DLG_RET_BACK                                    100
#define DLG_RET_OK_AUTOLUNCH                            101


namespace svx{
    namespace DocRecovery{


enum EDocStates
{
    /* TEMP STATES */

    /// default state, if a document was new created or loaded
    E_UNKNOWN = 0,
    /// modified against the original file
    E_MODIFIED = 1,
    /// an active document can be postponed to be saved later.
    E_POSTPONED = 2,
    /// was already handled during one AutoSave/Recovery session.
    E_HANDLED = 4,
    /** an action was started (saving/loading) ... Can be interesting later if the process may be was interrupted by an exception. */
    E_TRY_SAVE = 8,
    E_TRY_LOAD_BACKUP = 16,
    E_TRY_LOAD_ORIGINAL = 32,

    /* FINAL STATES */

    /// the Auto/Emergency saved document isn't useable any longer
    E_DAMAGED = 64,
    /// the Auto/Emergency saved document isnt really up-to-date (some changes can be missing)
    E_INCOMPLETE = 128,
    /// the Auto/Emergency saved document was processed successfully
    E_SUCCEDED = 512
};


enum ERecoveryState
{
    E_SUCCESSFULLY_RECOVERED,
    E_ORIGINAL_DOCUMENT_RECOVERED,
    E_RECOVERY_FAILED,
    E_RECOVERY_IS_IN_PROGRESS,
    E_NOT_RECOVERED_YET
};


struct TURLInfo
{
    public:

    /// unique ID, which is specified by the underlying autorecovery core!
    sal_Int32 ID;

    /// the full qualified document URL
    OUString OrgURL;

    /// the full qualified URL of the temp. file (if it's exists)
    OUString TempURL;

    /// a may be existing factory URL (e.g. for untitled documents)
    OUString FactoryURL;

    /// may be the document base on a template file !?
    OUString TemplateURL;

    /// the pure file name, without path, disc etcpp.
    OUString DisplayName;

    /// the application module, where this document was loaded
    OUString Module;

    /// state info as e.g. VALID, CORRUPTED, NON EXISTING ...
    sal_Int32 DocState;

    /// ui representation for DocState!
    ERecoveryState RecoveryState;

    /// standard icon
    Image StandardImage;

    public:

    TURLInfo()
        : ID           (-1                 )
        , DocState     (E_UNKNOWN          )
        , RecoveryState(E_NOT_RECOVERED_YET)
    {}
};


typedef ::std::vector< TURLInfo > TURLList;


class IRecoveryUpdateListener
{
    public:

        // inform listener about changed items, which should be refreshed
        virtual void updateItems() = 0;

        // inform listener about starting of the asynchronous recovery operation
        virtual void start() = 0;

        // inform listener about ending of the asynchronous recovery operation
        virtual void end() = 0;

        // TODO
        virtual void stepNext(TURLInfo* pItem) = 0;

    protected:
        ~IRecoveryUpdateListener() {}
};


class RecoveryCore : public ::cppu::WeakImplHelper1< css::frame::XStatusListener >
{

    // types, const
    public:


    // member
    private:

        /// TODO
        css::uno::Reference< css::uno::XComponentContext > m_xContext;

        /// TODO
        css::uno::Reference< css::frame::XDispatch > m_xRealCore;

        /// TODO
        css::uno::Reference< css::task::XStatusIndicator > m_xProgress;

        /// TODO
        TURLList m_lURLs;

        /// TODO
        IRecoveryUpdateListener* m_pListener;

        /** @short  knows the reason, why we listen on our internal m_xRealCore
                    member.

            @descr  Because we listen for different operations
                    on the core dispatch implementation, we must know,
                    which URL we have to use for deregistration!
         */
        bool m_bListenForSaving;


    // native interface
    public:


        /** @short  TODO */
        RecoveryCore(const css::uno::Reference< css::uno::XComponentContext >& rxContext,
                           bool                                            bUsedForSaving);


        /** @short  TODO */
        virtual ~RecoveryCore();


        /** @short  TODO */
        virtual css::uno::Reference< css::uno::XComponentContext > getComponentContext();


        /** @short  TODO */
        virtual TURLList* getURLListAccess();


        /** @short  TODO */
        static bool isBrokenTempEntry(const TURLInfo& rInfo);
        virtual void saveBrokenTempEntries(const OUString& sSaveDir);
        virtual void saveAllTempEntries(const OUString& sSaveDir);
        virtual void forgetBrokenTempEntries();
        virtual void forgetAllRecoveryEntries();
        void forgetBrokenRecoveryEntries();


        /** @short  TODO */
        virtual void setProgressHandler(const css::uno::Reference< css::task::XStatusIndicator >& xProgress);


        /** @short  TODO */
        virtual void setUpdateListener(IRecoveryUpdateListener* pListener);


        /** @short  TODO */
        virtual void doEmergencySavePrepare();
        virtual void doEmergencySave();
        virtual void doRecovery();


        /** @short  TODO */
        static ERecoveryState mapDocState2RecoverState(sal_Int32 eDocState);


    // uno interface
    public:

        // css.frame.XStatusListener
        virtual void SAL_CALL statusChanged(const css::frame::FeatureStateEvent& aEvent)
            throw(css::uno::RuntimeException, std::exception) SAL_OVERRIDE;

        // css.lang.XEventListener
        virtual void SAL_CALL disposing(const css::lang::EventObject& aEvent)
            throw(css::uno::RuntimeException, std::exception) SAL_OVERRIDE;


    // helper
    private:


        /** @short  starts listening on the internal EmergencySave/AutoRecovery core.
         */
        void impl_startListening();


        /** @short  stop listening on the internal EmergencySave/AutoRecovery core.
         */
        void impl_stopListening();


        /** @short  TODO */
        css::util::URL impl_getParsedURL(const OUString& sURL);
};


class PluginProgressWindow : public Window
{
    private:
        css::uno::Reference< css::lang::XComponent > m_xProgress;
    public:
        PluginProgressWindow(      Window*                                       pParent  ,
                             const css::uno::Reference< css::lang::XComponent >& xProgress);
        virtual ~PluginProgressWindow();
};

class PluginProgress : public ::cppu::WeakImplHelper2< css::task::XStatusIndicator ,
                                                       css::lang::XComponent       >
{
    // member
    private:
        /** @short  TODO */
        css::uno::Reference< css::task::XStatusIndicatorFactory > m_xProgressFactory;

        css::uno::Reference< css::task::XStatusIndicator > m_xProgress;

        PluginProgressWindow* m_pPlugProgressWindow;


    // native interface
    public:
        /** @short  TODO */
        PluginProgress(      Window*                                             pParent,
                       const css::uno::Reference< css::uno::XComponentContext >& xContext  );


        /** @short  TODO */
        virtual ~PluginProgress();


    // uno interface
    public:


        // XStatusIndicator
        virtual void SAL_CALL start(const OUString& sText ,
                                          sal_Int32        nRange)
            throw(css::uno::RuntimeException, std::exception) SAL_OVERRIDE;

        virtual void SAL_CALL end()
            throw(css::uno::RuntimeException, std::exception) SAL_OVERRIDE;

        virtual void SAL_CALL setText(const OUString& sText)
            throw(css::uno::RuntimeException, std::exception) SAL_OVERRIDE;

        virtual void SAL_CALL setValue(sal_Int32 nValue)
            throw(css::uno::RuntimeException, std::exception) SAL_OVERRIDE;

        virtual void SAL_CALL reset()
            throw(css::uno::RuntimeException, std::exception) SAL_OVERRIDE;


        // XComponent
        virtual void SAL_CALL dispose()
            throw(css::uno::RuntimeException, std::exception) SAL_OVERRIDE;

        virtual void SAL_CALL addEventListener(const css::uno::Reference< css::lang::XEventListener >& xListener)
            throw(css::uno::RuntimeException, std::exception) SAL_OVERRIDE;

        virtual void SAL_CALL removeEventListener( const css::uno::Reference< css::lang::XEventListener >& xListener)
            throw(css::uno::RuntimeException, std::exception) SAL_OVERRIDE;
};

class SaveDialog : public Dialog
{
    // member
    private:
        FixedText*      m_pTitleFT;
        ListBox*        m_pFileListLB;
        OKButton*       m_pOkBtn;
        RecoveryCore*   m_pCore;

    // interface
    public:
        /** @short  create all child controls of this dialog.

            @descr  The dialog isn't shown nor it starts any
                    action by itself!

            @param  pParent
                    can point to a parent window.
                    If its set to 0, the defmodal-dialog-parent
                    is used automatically.

            @param  pCore
                    provides access to the recovery core service
                    and the current list of open documents,
                    which should be shown inside this dialog.
         */
        SaveDialog(Window* pParent, RecoveryCore* pCore);

        DECL_LINK(OKButtonHdl, void*);
};

class SaveProgressDialog : public ModalDialog
                         , public IRecoveryUpdateListener
{
    // member
    private:
        OUString      m_aProgrBaseTxt;
        Window*       m_pProgrParent;

        // @short   TODO
        RecoveryCore* m_pCore;

        // @short   TODO
        css::uno::Reference< css::task::XStatusIndicator > m_xProgress;
    // interface
    public:
        /** @short  create all child controls of this dialog.

            @descr  The dialog isn't shown nor it starts any
                    action by itself!

            @param  pParent
                    can point to a parent window.
                    If its set to 0, the defmodal-dialog-parent
                    is used automatically.

            @param  pCore
                    used to start emegrency save.
         */
        SaveProgressDialog(Window*       pParent,
                           RecoveryCore* pCore  );

        /** @short  start the emergency save operation. */
        virtual short Execute() SAL_OVERRIDE;

        // IRecoveryUpdateListener
        virtual void updateItems() SAL_OVERRIDE;
        virtual void stepNext(TURLInfo* pItem) SAL_OVERRIDE;
        virtual void start() SAL_OVERRIDE;
        virtual void end() SAL_OVERRIDE;
};


class RecovDocListEntry : public SvLBoxString
{
public:


    /** @short TODO */
    RecovDocListEntry(      SvTreeListEntry* pEntry,
                            sal_uInt16       nFlags,
                      const OUString&      sText );


    /** @short TODO */
    virtual void Paint(
        const Point& rPos, SvTreeListBox& rOutDev, const SvViewDataEntry* pView, const SvTreeListEntry* pEntry) SAL_OVERRIDE;
};


class RecovDocList : public SvSimpleTable
{

    // member
    public:

        Image  m_aGreenCheckImg;
        Image  m_aYellowCheckImg;
        Image  m_aRedCrossImg;

        OUString m_aSuccessRecovStr;
        OUString m_aOrigDocRecovStr;
        OUString m_aRecovFailedStr;
        OUString m_aRecovInProgrStr;
        OUString m_aNotRecovYetStr;


    // interface
    public:


        /** @short TODO */
        RecovDocList(SvSimpleTableContainer& rParent, ResMgr& rResMgr);

        /** @short TODO */
        virtual ~RecovDocList();


        /** @short TODO */
        virtual void InitEntry(SvTreeListEntry* pEntry,
                               const OUString& rText,
                               const Image& rImage1,
                               const Image& rImage2,
                               SvLBoxButtonKind eButtonKind) SAL_OVERRIDE;
};


class RecoveryDialog : public Dialog
                     , public IRecoveryUpdateListener
{
    // member
    private:
        FixedText*      m_pTitleFT;
        FixedText*      m_pDescrFT;
        Window*         m_pProgrParent;
        RecovDocList*   m_pFileListLB;
        PushButton*     m_pNextBtn;
        CancelButton*   m_pCancelBtn;
        OUString        m_aTitleRecoveryInProgress;
        OUString        m_aRecoveryOnlyFinish;
        OUString        m_aRecoveryOnlyFinishDescr;

        RecoveryCore*   m_pCore;
        css::uno::Reference< css::task::XStatusIndicator > m_xProgress;
        enum EInternalRecoveryState
        {
            E_RECOVERY_PREPARED,            // dialog started ... recovery prepared
            E_RECOVERY_IN_PROGRESS,         // recovery core still in progress
            E_RECOVERY_CORE_DONE,           // recovery core finished it's task
            E_RECOVERY_DONE,                // user clicked "next" button
            E_RECOVERY_CANCELED,            // user clicked "cancel" button
            E_RECOVERY_CANCELED_BEFORE,     // user clicked "cancel" button before recovery was started
            E_RECOVERY_CANCELED_AFTERWARDS, // user clicked "cancel" button after reovery was finished
            E_RECOVERY_HANDLED              // the recovery wizard page was shown already ... and will be shown now again ...
        };
        sal_Int32 m_eRecoveryState;
        bool  m_bWaitForCore;
        bool  m_bWasRecoveryStarted;

    // member
    public:
        /** @short TODO */
        RecoveryDialog(Window*       pParent,
                       RecoveryCore* pCore  );

        virtual ~RecoveryDialog();

        // IRecoveryUpdateListener
        virtual void updateItems() SAL_OVERRIDE;
        virtual void stepNext(TURLInfo* pItem) SAL_OVERRIDE;
        virtual void start() SAL_OVERRIDE;
        virtual void end() SAL_OVERRIDE;

        short execute();

    // helper
    private:
        /** @short TODO */
        DECL_LINK(NextButtonHdl, void*);
        DECL_LINK(CancelButtonHdl, void*);


        /** @short TODO */
        OUString impl_getStatusString( const TURLInfo& rInfo ) const;
};


class BrokenRecoveryDialog : public ModalDialog
{

    // member
    private:
        ListBox         *m_pFileListLB;
        Edit            *m_pSaveDirED;
        PushButton      *m_pSaveDirBtn;
        PushButton        *m_pOkBtn;
        CancelButton    *m_pCancelBtn;

        OUString m_sSavePath;
        RecoveryCore*   m_pCore;
        bool        m_bBeforeRecovery;
        bool        m_bExecutionNeeded;


    // interface
    public:


        /** @short TODO */
        BrokenRecoveryDialog(Window*       pParent        ,
                             RecoveryCore* pCore          ,
                             bool      bBeforeRecovery);


        /** @short TODO */
        virtual ~BrokenRecoveryDialog();


        /** @short TODO */
        virtual bool isExecutionNeeded();


        /** @short TODO */
        virtual OUString getSaveDirURL();


    // helper
    private:


        /** @short TODO */
        void impl_refresh();


        /** @short TODO */
        DECL_LINK(SaveButtonHdl, void*);


        /** @short TODO */
        DECL_LINK(OkButtonHdl, void*);


        /** @short TODO */
        DECL_LINK(CancelButtonHdl, void*);


        /** @short TODO */
        void impl_askForSavePath();
};
    }   // namespace DocRecovery
}   // namespace svx

#endif

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
