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

#ifndef INCLUDED_FORMULA_FORMULA_HXX
#define INCLUDED_FORMULA_FORMULA_HXX

#include <sfx2/basedlgs.hxx>
#include <memory>
#include <formula/formuladllapi.h>
#include <formula/omoduleclient.hxx>
#include <formula/IFunctionDescription.hxx>

namespace formula
{

#define STRUCT_END    1
#define STRUCT_FOLDER 2
#define STRUCT_ERROR  3

enum FormulaDlgMode { FORMULA_FORMDLG_FORMULA, FORMULA_FORMDLG_ARGS, FORMULA_FORMDLG_EDIT };



class FormulaDlg_Impl;
class IControlReferenceHandler;
class IFunctionDescription;
class IFunctionManager;
class FormulaHelper;
class RefEdit;
class RefButton;
class FormEditData;

class FORMULA_DLLPUBLIC FormulaModalDialog :   public ModalDialog, public formula::IFormulaEditorHelper
{
    friend class FormulaDlg_Impl;
public:
                    FormulaModalDialog( Window* pParent
                                            , bool _bSupportFunctionResult
                                            , bool _bSupportResult
                                            , bool _bSupportMatrix
                                            ,IFunctionManager* _pFunctionMgr
                                            ,IControlReferenceHandler* _pDlg = NULL );
    virtual ~FormulaModalDialog();

private:
    SAL_WNODEPRECATED_DECLARATIONS_PUSH
    ::std::auto_ptr<FormulaDlg_Impl> m_pImpl;
    SAL_WNODEPRECATED_DECLARATIONS_POP

protected:

    virtual bool    PreNotify( NotifyEvent& rNEvt ) SAL_OVERRIDE;
    ::std::pair<RefButton*,RefEdit*> RefInputStartBefore( RefEdit* pEdit, RefButton* pButton = NULL );
    void            RefInputStartAfter( RefEdit* pEdit, RefButton* pButton = NULL );
    void            RefInputDoneAfter( bool bForced = false );
    void            SetFocusWin(Window *pWin,const OString& nUniqueId);

    void            SetMeText(const OUString& _sText);
    void            Update();
    bool            CheckMatrix(OUString& aFormula /*IN/OUT*/);
    void            Update(const OUString& _sExp);

    void            StoreFormEditData(FormEditData* pData);
};

class FORMULA_DLLPUBLIC FormulaDlg:
    private OModuleClient, public SfxModelessDialog, public IFormulaEditorHelper
        // order of base classes is important, as OModuleClient controls the
        // lifecycle of the ResMgr passed into SfxModelessDialog (via
        // formula::ModuleRes), and at least with DBG_UTIL calling TestRes in
        // ~Resource, the ResMgr must outlive the Resource (from which
        // SfxModelessDialog ultimately derives)
{
    friend class FormulaDlg_Impl;
public:
                    FormulaDlg( SfxBindings* pB
                                    , SfxChildWindow* pCW
                                    , Window* pParent
                                    , bool _bSupportFunctionResult
                                    , bool _bSupportResult
                                    , bool _bSupportMatrix
                                    , IFunctionManager* _pFunctionMgr
                                    , IControlReferenceHandler* _pDlg = NULL );
    virtual ~FormulaDlg();
private:
    SAL_WNODEPRECATED_DECLARATIONS_PUSH
    ::std::auto_ptr<FormulaDlg_Impl> m_pImpl;
    SAL_WNODEPRECATED_DECLARATIONS_POP

    DECL_LINK( UpdateFocusHdl, void*);
protected:
    void            disableOk();

protected:

    virtual bool    PreNotify( NotifyEvent& rNEvt ) SAL_OVERRIDE;
    ::std::pair<RefButton*,RefEdit*> RefInputStartBefore( RefEdit* pEdit, RefButton* pButton = NULL );
    void            RefInputStartAfter( RefEdit* pEdit, RefButton* pButton = NULL );
    void            RefInputDoneAfter( bool bForced = false );
    void            SetFocusWin(Window *pWin,const OString& nUniqueId);
    void            HighlightFunctionParas(const OUString& aFormula);

    void            SetMeText(const OUString& _sText);
    FormulaDlgMode SetMeText(const OUString& _sText, sal_Int32 PrivStart, sal_Int32 PrivEnd, bool bMatrix, bool _bSelect, bool _bUpdate);
    void            Update();
    bool            CheckMatrix(OUString& aFormula /*IN/OUT*/);
    OUString        GetMeText() const;
    void            Update(const OUString& _sExp);
    void            DoEnter(bool _bOk);
    const IFunctionDescription* getCurrentFunctionDescription() const;
    bool            UpdateParaWin(Selection& _rSelection);
    void            UpdateParaWin(const Selection& _rSelection, const OUString& _sRefStr);
    RefEdit*        GetActiveEdit();
    void            SetEdSelection();

    void            StoreFormEditData(FormEditData* pData);

    const FormulaHelper& GetFormulaHelper() const;
};

} // formula

#endif // INCLUDED_FORMULA_FORMULA_HXX

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
