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

#ifndef INCLUDED_SC_SOURCE_UI_INC_AUTOFMT_HXX
#define INCLUDED_SC_SOURCE_UI_INC_AUTOFMT_HXX

#include <vcl/virdev.hxx>
#include <vcl/fixed.hxx>
#include <vcl/lstbox.hxx>
#include <vcl/button.hxx>
#include <vcl/morebtn.hxx>
#include <vcl/dialog.hxx>
#include <svtools/scriptedtext.hxx>
#include <svx/framelinkarray.hxx>
#include "scdllapi.h"
#include "viewdata.hxx"

class ScAutoFormatData;
class SvxBoxItem;
class SvxLineItem;
class ScAutoFmtPreview; // s.u.
class SvNumberFormatter;
class ScDocument;

enum AutoFmtLine { TOP_LINE, BOTTOM_LINE, LEFT_LINE, RIGHT_LINE };

class SC_DLLPUBLIC ScAutoFmtPreview : public Window
{
public:
    ScAutoFmtPreview(Window* pParent);
    void DetectRTL(ScViewData *pViewData);
    virtual ~ScAutoFmtPreview();

    void NotifyChange( ScAutoFormatData* pNewData );

protected:
    virtual void Paint(const Rectangle& rRect) SAL_OVERRIDE;
    virtual void Resize() SAL_OVERRIDE;

private:
    ScAutoFormatData*       pCurData;
    VirtualDevice           aVD;
    SvtScriptedTextHelper   aScriptedText;
    ::com::sun::star::uno::Reference< ::com::sun::star::i18n::XBreakIterator > xBreakIter;
    bool                    bFitWidth;
    svx::frame::Array       maArray;            /// Implementation to draw the frame borders.
    bool                    mbRTL;
    Size                    aPrvSize;
    long                    mnLabelColWidth;
    long                    mnDataColWidth1;
    long                    mnDataColWidth2;
    long                    mnRowHeight;
    const OUString          aStrJan;
    const OUString          aStrFeb;
    const OUString          aStrMar;
    const OUString          aStrNorth;
    const OUString          aStrMid;
    const OUString          aStrSouth;
    const OUString          aStrSum;
    SvNumberFormatter*      pNumFmt;

    SAL_DLLPRIVATE void  Init            ();
    SAL_DLLPRIVATE void  DoPaint         ( const Rectangle& rRect );
    SAL_DLLPRIVATE void  CalcCellArray   ( bool bFitWidth );
    SAL_DLLPRIVATE void  CalcLineMap     ();
    SAL_DLLPRIVATE void  PaintCells      ();

/*  Usage of type size_t instead of SCCOL/SCROW is correct here - used in
    conjunction with class svx::frame::Array (svx/framelinkarray.hxx), which
    expects size_t coordinates. */

    SAL_DLLPRIVATE sal_uInt16              GetFormatIndex( size_t nCol, size_t nRow ) const;
    SAL_DLLPRIVATE const SvxBoxItem&   GetBoxItem( size_t nCol, size_t nRow ) const;
    SAL_DLLPRIVATE const SvxLineItem&  GetDiagItem( size_t nCol, size_t nRow, bool bTLBR ) const;

    SAL_DLLPRIVATE void                DrawString( size_t nCol, size_t nRow );
    SAL_DLLPRIVATE void                DrawStrings();
    SAL_DLLPRIVATE void                DrawBackground();

    SAL_DLLPRIVATE void    MakeFonts       ( sal_uInt16 nIndex,
                              Font& rFont,
                              Font& rCJKFont,
                              Font& rCTLFont );

    SAL_DLLPRIVATE void CheckPriority    ( sal_uInt16            nCurLine,
                              AutoFmtLine       eLine,
                              ::editeng::SvxBorderLine& rLine );
    SAL_DLLPRIVATE void  GetLines        ( sal_uInt16 nIndex, AutoFmtLine eLine,
                              ::editeng::SvxBorderLine& rLineD,
                              ::editeng::SvxBorderLine& rLineLT,
                              ::editeng::SvxBorderLine& rLineL,
                              ::editeng::SvxBorderLine& rLineLB,
                              ::editeng::SvxBorderLine& rLineRT,
                              ::editeng::SvxBorderLine& rLineR,
                              ::editeng::SvxBorderLine& rLineRB );
};

#endif // INCLUDED_SC_SOURCE_UI_INC_AUTOFMT_HXX

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
