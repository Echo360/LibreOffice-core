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

#ifndef INCLUDED_FORMULA_SOURCE_UI_DLG_STRUCTPG_HXX
#define INCLUDED_FORMULA_SOURCE_UI_DLG_STRUCTPG_HXX

#include <svtools/stdctrl.hxx>
#include <vcl/lstbox.hxx>
#include <vcl/group.hxx>
#include <svtools/svmedit.hxx>
#include <vcl/tabpage.hxx>
#include <vcl/tabctrl.hxx>
#include <svtools/treelistbox.hxx>
#include "formula/IFunctionDescription.hxx"
#include "formula/omoduleclient.hxx"




namespace formula
{

class IFormulaToken;
class   StructListBox : public SvTreeListBox
{
private:

    bool            bActiveFlag;

protected:
                    virtual void MouseButtonDown( const MouseEvent& rMEvt ) SAL_OVERRIDE;

public:

                    StructListBox(Window* pParent, WinBits nBits );

    /** Inserts an entry with static image (no difference between collapsed/expanded). */
    SvTreeListEntry*    InsertStaticEntry(
                        const OUString& rText,
                        const Image& rEntryImg,
                        SvTreeListEntry* pParent = NULL,
                        sal_uLong nPos = TREELIST_APPEND,
                        IFormulaToken* pToken = NULL );

    void            SetActiveFlag(bool bFlag=true);
    bool            GetActiveFlag() { return bActiveFlag;}
    void            GetFocus() SAL_OVERRIDE;
    void            LoseFocus() SAL_OVERRIDE;
};



class StructPage : public TabPage
                    , public IStructHelper
{
private:
    OModuleClient   m_aModuleClient;
    Link            aSelLink;

    StructListBox   *m_pTlbStruct;
    Image           maImgEnd;
    Image           maImgError;

    IFormulaToken*  pSelectedToken;

    DECL_LINK( SelectHdl, SvTreeListBox* );

    using Window::GetParent;

protected:

    IFormulaToken*      GetFunctionEntry(SvTreeListEntry* pEntry);

public:

                    StructPage( Window* pParent);

    void            ClearStruct();
    virtual SvTreeListEntry*    InsertEntry(const OUString& rText, SvTreeListEntry* pParent,
                                sal_uInt16 nFlag,sal_uLong nPos=0,IFormulaToken* pScToken=NULL) SAL_OVERRIDE;

    virtual OUString            GetEntryText(SvTreeListEntry* pEntry) const SAL_OVERRIDE;
    virtual SvTreeListEntry*    GetParent(SvTreeListEntry* pEntry) const SAL_OVERRIDE;

    void            SetSelectionHdl( const Link& rLink ) { aSelLink = rLink; }
    const Link&     GetSelectionHdl() const { return aSelLink; }
};

} // formula

#endif

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
