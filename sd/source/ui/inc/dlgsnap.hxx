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

#ifndef INCLUDED_SD_SOURCE_UI_INC_DLGSNAP_HXX
#define INCLUDED_SD_SOURCE_UI_INC_DLGSNAP_HXX

#include <vcl/button.hxx>
#include <vcl/fixed.hxx>
#include <vcl/field.hxx>
#include <vcl/group.hxx>
#include <vcl/layout.hxx>
#include <tools/fract.hxx>
#include <vcl/dialog.hxx>
#include "sdenumdef.hxx"
/************************************************************************/

class SfxItemSet;
namespace sd {
    class View;
}

/**
 * dialog to adjust snap- lines and points
 */
class SdSnapLineDlg : public ModalDialog
{
private:
    FixedText*          m_pFtX;
    MetricField*        m_pMtrFldX;
    FixedText*          m_pFtY;
    MetricField*        m_pMtrFldY;
    VclContainer*       m_pRadioGroup;
    RadioButton*        m_pRbPoint;
    RadioButton*        m_pRbVert;
    RadioButton*        m_pRbHorz;
    PushButton*         m_pBtnDelete;
    long                nXValue;
    long                nYValue;
    FieldUnit           eUIUnit;
    Fraction            aUIScale;

    DECL_LINK( ClickHdl, Button * );

public:
    SdSnapLineDlg(Window* pWindow, const SfxItemSet& rInAttrs, ::sd::View* pView);

    void GetAttr(SfxItemSet& rOutAttrs);

    void HideRadioGroup();
    void HideDeleteBtn() { m_pBtnDelete->Hide(); }
    void SetInputFields(bool bEnableX, bool bEnableY);
};

#endif // INCLUDED_SD_SOURCE_UI_INC_DLGSNAP_HXX

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
