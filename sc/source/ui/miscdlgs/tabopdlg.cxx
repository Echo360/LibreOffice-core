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

#include "scitems.hxx"
#include <sfx2/dispatch.hxx>
#include <vcl/layout.hxx>

#include "uiitems.hxx"
#include "global.hxx"
#include "document.hxx"
#include "scresid.hxx"
#include "sc.hrc"
#include "reffact.hxx"

#include "tabopdlg.hxx"

//  class ScTabOpDlg

ScTabOpDlg::ScTabOpDlg( SfxBindings* pB, SfxChildWindow* pCW, Window* pParent,
                        ScDocument*         pDocument,
                        const ScRefAddress& rCursorPos )

    : ScAnyRefDlg(pB, pCW, pParent, "MultipleOperationsDialog",
        "modules/scalc/ui/multipleoperationsdialog.ui")
    , theFormulaCell(rCursorPos)
    , pDoc(pDocument)
    , nCurTab(theFormulaCell.Tab())
    , pEdActive(NULL)
    , bDlgLostFocus(false)
    , errMsgNoFormula(ScResId(STR_NOFORMULASPECIFIED))
    , errMsgNoColRow(ScResId(STR_NOCOLROW))
    , errMsgWrongFormula(ScResId(STR_WRONGFORMULA))
    , errMsgWrongRowCol(ScResId(STR_WRONGROWCOL))
    , errMsgNoColFormula(ScResId(STR_NOCOLFORMULA))
    , errMsgNoRowFormula(ScResId(STR_NOROWFORMULA))
{
    get(m_pFtFormulaRange, "formulasft");
    get(m_pEdFormulaRange, "formulas");
    m_pEdFormulaRange->SetReferences(this, m_pFtFormulaRange);
    get(m_pRBFormulaRange, "formulasref");
    m_pRBFormulaRange->SetReferences(this, m_pEdFormulaRange);

    get(m_pFtRowCell, "rowft");
    get(m_pEdRowCell, "row");
    m_pEdRowCell->SetReferences(this, m_pFtRowCell);
    get(m_pRBRowCell, "rowref");
    m_pRBRowCell->SetReferences(this, m_pEdRowCell);

    get(m_pFtColCell, "colft");
    get(m_pEdColCell, "col");
    m_pEdColCell->SetReferences(this, m_pFtColCell);
    get(m_pRBColCell, "colref");
    m_pRBColCell->SetReferences(this, m_pEdColCell);

    get(m_pBtnOk, "ok");
    get(m_pBtnCancel, "cancel");

    Init();
}

ScTabOpDlg::~ScTabOpDlg()
{
    Hide();
}

void ScTabOpDlg::Init()
{
    m_pBtnOk->SetClickHdl     ( LINK( this, ScTabOpDlg, BtnHdl ) );
    m_pBtnCancel->SetClickHdl     ( LINK( this, ScTabOpDlg, BtnHdl ) );

    Link aLink = LINK( this, ScTabOpDlg, GetFocusHdl );
    m_pEdFormulaRange->SetGetFocusHdl( aLink );
    m_pRBFormulaRange->SetGetFocusHdl( aLink );
    m_pEdRowCell->SetGetFocusHdl( aLink );
    m_pRBRowCell->SetGetFocusHdl( aLink );
    m_pEdColCell->SetGetFocusHdl( aLink );
    m_pRBColCell->SetGetFocusHdl( aLink );

    aLink = LINK( this, ScTabOpDlg, LoseFocusHdl );
    m_pEdFormulaRange->SetLoseFocusHdl( aLink );
    m_pRBFormulaRange->SetLoseFocusHdl( aLink );
    m_pEdRowCell->SetLoseFocusHdl( aLink );
    m_pRBRowCell->SetLoseFocusHdl( aLink );
    m_pEdColCell->SetLoseFocusHdl( aLink );
    m_pRBColCell->SetLoseFocusHdl( aLink );

    m_pEdFormulaRange->GrabFocus();
    pEdActive = m_pEdFormulaRange;

    //@BugID 54702 Enablen/Disablen nur noch in Basisklasse
    //SFX_APPWINDOW->Enable();
}

bool ScTabOpDlg::Close()
{
    return DoClose( ScTabOpDlgWrapper::GetChildWindowId() );
}

void ScTabOpDlg::SetActive()
{
    if ( bDlgLostFocus )
    {
        bDlgLostFocus = false;
        if( pEdActive )
            pEdActive->GrabFocus();
    }
    else
        GrabFocus();

    RefInputDone();
}

void ScTabOpDlg::SetReference( const ScRange& rRef, ScDocument* pDocP )
{
    if ( pEdActive )
    {
        ScAddress::Details aDetails(pDocP->GetAddressConvention(), 0, 0);

        if ( rRef.aStart != rRef.aEnd )
            RefInputStart(pEdActive);

        OUString      aStr;
        sal_uInt16      nFmt = ( rRef.aStart.Tab() == nCurTab )
                                ? SCR_ABS
                                : SCR_ABS_3D;

        if (pEdActive == m_pEdFormulaRange)
        {
            theFormulaCell.Set( rRef.aStart, false, false, false);
            theFormulaEnd.Set( rRef.aEnd, false, false, false);
            aStr = rRef.Format(nFmt, pDocP, aDetails);
        }
        else if ( pEdActive == m_pEdRowCell )
        {
            theRowCell.Set( rRef.aStart, false, false, false);
            aStr = rRef.aStart.Format(nFmt, pDocP, aDetails);
        }
        else if ( pEdActive == m_pEdColCell )
        {
            theColCell.Set( rRef.aStart, false, false, false);
            aStr = rRef.aStart.Format(nFmt, pDocP, aDetails);
        }

        pEdActive->SetRefString( aStr );
    }
}

void ScTabOpDlg::RaiseError( ScTabOpErr eError )
{
    const OUString* pMsg = &errMsgNoFormula;
    Edit*           pEd  = m_pEdFormulaRange;

    switch ( eError )
    {
        case TABOPERR_NOFORMULA:
            pMsg = &errMsgNoFormula;
            pEd  = m_pEdFormulaRange;
            break;

        case TABOPERR_NOCOLROW:
            pMsg = &errMsgNoColRow;
            pEd  = m_pEdRowCell;
            break;

        case TABOPERR_WRONGFORMULA:
            pMsg = &errMsgWrongFormula;
            pEd  = m_pEdFormulaRange;
            break;

        case TABOPERR_WRONGROW:
            pMsg = &errMsgWrongRowCol;
            pEd  = m_pEdRowCell;
            break;

        case TABOPERR_NOCOLFORMULA:
            pMsg = &errMsgNoColFormula;
            pEd  = m_pEdFormulaRange;
            break;

        case TABOPERR_WRONGCOL:
            pMsg = &errMsgWrongRowCol;
            pEd  = m_pEdColCell;
            break;

        case TABOPERR_NOROWFORMULA:
            pMsg = &errMsgNoRowFormula;
            pEd  = m_pEdFormulaRange;
            break;
    }

    MessageDialog(this, *pMsg, VCL_MESSAGE_ERROR, VCL_BUTTONS_OK_CANCEL).Execute();
    pEd->GrabFocus();
}

static bool lcl_Parse( const OUString& rString, ScDocument* pDoc, SCTAB nCurTab,
                ScRefAddress& rStart, ScRefAddress& rEnd )
{
    bool bRet = false;
    const formula::FormulaGrammar::AddressConvention eConv = pDoc->GetAddressConvention();
    if ( rString.indexOf(':') != -1 )
        bRet = ConvertDoubleRef( pDoc, rString, nCurTab, rStart, rEnd, eConv );
    else
    {
        bRet = ConvertSingleRef( pDoc, rString, nCurTab, rStart, eConv );
        rEnd = rStart;
    }
    return bRet;
}

// Handler:

IMPL_LINK( ScTabOpDlg, BtnHdl, PushButton*, pBtn )
{
    if (pBtn == m_pBtnOk)
    {
        ScTabOpParam::Mode eMode = ScTabOpParam::Column;
        sal_uInt16 nError = 0;

        // Zu ueberpruefen:
        // 1. enthalten die Strings korrekte Tabellenkoordinaten/def.Namen?
        // 2. IstFormelRang Zeile bei leerer Zeile bzw. Spalte bei leerer Spalte
        //    bzw. Einfachreferenz bei beidem?
        // 3. Ist mindestens Zeile oder Spalte und Formel voll?

        if (m_pEdFormulaRange->GetText().isEmpty())
            nError = TABOPERR_NOFORMULA;
        else if (m_pEdRowCell->GetText().isEmpty() &&
                 m_pEdColCell->GetText().isEmpty())
            nError = TABOPERR_NOCOLROW;
        else if ( !lcl_Parse( m_pEdFormulaRange->GetText(), pDoc, nCurTab,
                                theFormulaCell, theFormulaEnd ) )
            nError = TABOPERR_WRONGFORMULA;
        else
        {
            const formula::FormulaGrammar::AddressConvention eConv = pDoc->GetAddressConvention();
            if (!m_pEdRowCell->GetText().isEmpty())
            {
                if (!ConvertSingleRef( pDoc, m_pEdRowCell->GetText(), nCurTab,
                                       theRowCell, eConv ))
                    nError = TABOPERR_WRONGROW;
                else
                {
                    if (m_pEdColCell->GetText().isEmpty() &&
                        theFormulaCell.Col() != theFormulaEnd.Col())
                        nError = TABOPERR_NOCOLFORMULA;
                    else
                        eMode = ScTabOpParam::Row;
                }
            }
            if (!m_pEdColCell->GetText().isEmpty())
            {
                if (!ConvertSingleRef( pDoc, m_pEdColCell->GetText(), nCurTab,
                                       theColCell, eConv ))
                    nError = TABOPERR_WRONGCOL;
                else
                {
                    if (eMode == ScTabOpParam::Row)                         // beides
                    {
                        eMode = ScTabOpParam::Both;
                        ConvertSingleRef( pDoc, m_pEdFormulaRange->GetText(), nCurTab,
                                          theFormulaCell, eConv );
                    }
                    else if (theFormulaCell.Row() != theFormulaEnd.Row())
                        nError = TABOPERR_NOROWFORMULA;
                    else
                        eMode = ScTabOpParam::Column;
                }
            }
        }

        if (nError)
            RaiseError( (ScTabOpErr) nError );
        else
        {
            ScTabOpParam aOutParam(theFormulaCell, theFormulaEnd, theRowCell, theColCell, eMode);
            ScTabOpItem  aOutItem( SID_TABOP, &aOutParam );

            SetDispatcherLock( false );
            SwitchToDocument();
            GetBindings().GetDispatcher()->Execute( SID_TABOP,
                                      SFX_CALLMODE_SLOT | SFX_CALLMODE_RECORD,
                                      &aOutItem, 0L, 0L );
            Close();
        }
    }
    else if (pBtn == m_pBtnCancel)
        Close();

    return 0;
}

IMPL_LINK( ScTabOpDlg, GetFocusHdl, Control*, pCtrl )
{
    if( (pCtrl == (Control*)m_pEdFormulaRange) || (pCtrl == (Control*)m_pRBFormulaRange) )
        pEdActive = m_pEdFormulaRange;
    else if( (pCtrl == (Control*)m_pEdRowCell) || (pCtrl == (Control*)m_pRBRowCell) )
        pEdActive = m_pEdRowCell;
    else if( (pCtrl == (Control*)m_pEdColCell) || (pCtrl == (Control*)m_pRBColCell) )
        pEdActive = m_pEdColCell;
    else
        pEdActive = NULL;

    if( pEdActive )
        pEdActive->SetSelection( Selection( 0, SELECTION_MAX ) );

    return 0;
}

IMPL_LINK_NOARG(ScTabOpDlg, LoseFocusHdl)
{
    bDlgLostFocus = !IsActive();
    return 0;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
