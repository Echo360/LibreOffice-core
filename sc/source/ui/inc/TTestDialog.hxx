/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 */

#ifndef INCLUDED_SC_SOURCE_UI_INC_TTESTDIALOG_HXX
#define INCLUDED_SC_SOURCE_UI_INC_TTESTDIALOG_HXX

#include "StatisticsTwoVariableDialog.hxx"

class ScTTestDialog : public ScStatisticsTwoVariableDialog
{
public:
    ScTTestDialog(
        SfxBindings* pB, SfxChildWindow* pCW,
        Window* pParent, ScViewData* pViewData );

    virtual ~ScTTestDialog();

    virtual bool Close() SAL_OVERRIDE;

protected:
    virtual sal_Int16 GetUndoNameId() SAL_OVERRIDE;
    virtual ScRange ApplyOutput(ScDocShell* pDocShell) SAL_OVERRIDE;
};

#endif

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
