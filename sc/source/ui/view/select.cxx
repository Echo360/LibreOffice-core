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

#include <tools/urlobj.hxx>
#include <vcl/svapp.hxx>
#include <sfx2/docfile.hxx>

#include "select.hxx"
#include "sc.hrc"
#include "tabvwsh.hxx"
#include "scmod.hxx"
#include "document.hxx"
#include "transobj.hxx"
#include "docsh.hxx"
#include "tabprotection.hxx"
#include "markdata.hxx"
#include <gridwin.hxx>

#if defined WNT
#define SC_SELENG_REFMODE_UPDATE_INTERVAL_MIN 65
#endif

extern sal_uInt16 nScFillModeMouseModifier;             // global.cxx

using namespace com::sun::star;

// STATIC DATA -----------------------------------------------------------

static Point aSwitchPos;                //! Member
static bool bDidSwitch = false;

// View (Gridwin / keyboard)
ScViewFunctionSet::ScViewFunctionSet( ScViewData* pNewViewData ) :
        pViewData( pNewViewData ),
        pEngine( NULL ),
        bAnchor( false ),
        bStarted( false )
{
    OSL_ENSURE(pViewData, "ViewData==0 at FunctionSet");
}

ScSplitPos ScViewFunctionSet::GetWhich()
{
    if (pEngine)
        return pEngine->GetWhich();
    else
        return pViewData->GetActivePart();
}

sal_uLong ScViewFunctionSet::CalcUpdateInterval( const Size& rWinSize, const Point& rEffPos,
                                             bool bLeftScroll, bool bTopScroll, bool bRightScroll, bool bBottomScroll )
{
    sal_uLong nUpdateInterval = SELENG_AUTOREPEAT_INTERVAL_MAX;
    Window* pWin = pEngine->GetWindow();
    Rectangle aScrRect = pWin->GetDesktopRectPixel();
    Point aRootPos = pWin->OutputToAbsoluteScreenPixel(Point(0,0));
    if (bRightScroll)
    {
        double nWinRight = rWinSize.getWidth() + aRootPos.getX();
        double nMarginRight = aScrRect.GetWidth() - nWinRight;
        double nHOffset = rEffPos.X() - rWinSize.Width();
        double nHAccelRate = nHOffset / nMarginRight;

        if (nHAccelRate > 1.0)
            nHAccelRate = 1.0;

        nUpdateInterval = static_cast<sal_uLong>(SELENG_AUTOREPEAT_INTERVAL_MAX*(1.0 - nHAccelRate));
    }

    if (bLeftScroll)
    {
        double nMarginLeft = aRootPos.getX();
        double nHOffset = -rEffPos.X();
        double nHAccelRate = nHOffset / nMarginLeft;

        if (nHAccelRate > 1.0)
            nHAccelRate = 1.0;

        sal_uLong nTmp = static_cast<sal_uLong>(SELENG_AUTOREPEAT_INTERVAL_MAX*(1.0 - nHAccelRate));
        if (nUpdateInterval > nTmp)
            nUpdateInterval = nTmp;
    }

    if (bBottomScroll)
    {
        double nWinBottom = rWinSize.getHeight() + aRootPos.getY();
        double nMarginBottom = aScrRect.GetHeight() - nWinBottom;
        double nVOffset = rEffPos.Y() - rWinSize.Height();
        double nVAccelRate = nVOffset / nMarginBottom;

        if (nVAccelRate > 1.0)
            nVAccelRate = 1.0;

        sal_uLong nTmp = static_cast<sal_uLong>(SELENG_AUTOREPEAT_INTERVAL_MAX*(1.0 - nVAccelRate));
        if (nUpdateInterval > nTmp)
            nUpdateInterval = nTmp;
    }

    if (bTopScroll)
    {
        double nMarginTop = aRootPos.getY();
        double nVOffset = -rEffPos.Y();
        double nVAccelRate = nVOffset / nMarginTop;

        if (nVAccelRate > 1.0)
            nVAccelRate = 1.0;

        sal_uLong nTmp = static_cast<sal_uLong>(SELENG_AUTOREPEAT_INTERVAL_MAX*(1.0 - nVAccelRate));
        if (nUpdateInterval > nTmp)
            nUpdateInterval = nTmp;
    }

#ifdef WNT
    ScTabViewShell* pViewShell = pViewData->GetViewShell();
    bool bRefMode = pViewShell && pViewShell->IsRefInputMode();
    if (bRefMode && nUpdateInterval < SC_SELENG_REFMODE_UPDATE_INTERVAL_MIN)
        // Lower the update interval during ref mode, because re-draw can be
        // expensive on Windows.  Making this interval too small would queue up
        // the scroll/paint requests which would cause semi-infinite
        // scrolls even after the mouse cursor is released.  We don't have
        // this problem on Linux.
        nUpdateInterval = SC_SELENG_REFMODE_UPDATE_INTERVAL_MIN;
#endif
    return nUpdateInterval;
}

void ScViewFunctionSet::SetSelectionEngine( ScViewSelectionEngine* pSelEngine )
{
    pEngine = pSelEngine;
}

// Drag & Drop
void ScViewFunctionSet::BeginDrag()
{
    SCTAB nTab = pViewData->GetTabNo();

    SCsCOL nPosX;
    SCsROW nPosY;
    if (pEngine)
    {
        Point aMPos = pEngine->GetMousePosPixel();
        pViewData->GetPosFromPixel( aMPos.X(), aMPos.Y(), GetWhich(), nPosX, nPosY );
    }
    else
    {
        nPosX = pViewData->GetCurX();
        nPosY = pViewData->GetCurY();
    }

    ScModule* pScMod = SC_MOD();
    bool bRefMode = pScMod->IsFormulaMode();
    if (!bRefMode)
    {
        pViewData->GetView()->FakeButtonUp( GetWhich() );   // ButtonUp is swallowed

        ScMarkData& rMark = pViewData->GetMarkData();
        rMark.MarkToSimple();
        if ( rMark.IsMarked() && !rMark.IsMultiMarked() )
        {
            ScDocument* pClipDoc = new ScDocument( SCDOCMODE_CLIP );
            // bApi = TRUE -> no error messages
            bool bCopied = pViewData->GetView()->CopyToClip( pClipDoc, false, true );
            if ( bCopied )
            {
                sal_Int8 nDragActions = pViewData->GetView()->SelectionEditable() ?
                                        ( DND_ACTION_COPYMOVE | DND_ACTION_LINK ) :
                                        ( DND_ACTION_COPY | DND_ACTION_LINK );

                ScDocShell* pDocSh = pViewData->GetDocShell();
                TransferableObjectDescriptor aObjDesc;
                pDocSh->FillTransferableObjectDescriptor( aObjDesc );
                aObjDesc.maDisplayName = pDocSh->GetMedium()->GetURLObject().GetURLNoPass();
                // maSize is set in ScTransferObj ctor

                ScTransferObj* pTransferObj = new ScTransferObj( pClipDoc, aObjDesc );
                uno::Reference<datatransfer::XTransferable> xTransferable( pTransferObj );

                // set position of dragged cell within range
                ScRange aMarkRange = pTransferObj->GetRange();
                SCCOL nStartX = aMarkRange.aStart.Col();
                SCROW nStartY = aMarkRange.aStart.Row();
                SCCOL nHandleX = (nPosX >= (SCsCOL) nStartX) ? nPosX - nStartX : 0;
                SCROW nHandleY = (nPosY >= (SCsROW) nStartY) ? nPosY - nStartY : 0;
                pTransferObj->SetDragHandlePos( nHandleX, nHandleY );
                pTransferObj->SetVisibleTab( nTab );

                pTransferObj->SetDragSource( pDocSh, rMark );

                Window* pWindow = pViewData->GetActiveWin();
                if ( pWindow->IsTracking() )
                    pWindow->EndTracking( ENDTRACK_CANCEL );    // abort selecting

                SC_MOD()->SetDragObject( pTransferObj, NULL );      // for internal D&D
                pTransferObj->StartDrag( pWindow, nDragActions );

                return;         // dragging started
            }
            else
                delete pClipDoc;
        }
    }

}

// Selection
void ScViewFunctionSet::CreateAnchor()
{
    if (bAnchor) return;

    bool bRefMode = SC_MOD()->IsFormulaMode();
    if (bRefMode)
        SetAnchor( pViewData->GetRefStartX(), pViewData->GetRefStartY() );
    else
        SetAnchor( pViewData->GetCurX(), pViewData->GetCurY() );
}

void ScViewFunctionSet::SetAnchor( SCCOL nPosX, SCROW nPosY )
{
    bool bRefMode = SC_MOD()->IsFormulaMode();
    ScTabView* pView = pViewData->GetView();
    SCTAB nTab = pViewData->GetTabNo();

    if (bRefMode)
    {
        pView->DoneRefMode( false );
        aAnchorPos.Set( nPosX, nPosY, nTab );
        pView->InitRefMode( aAnchorPos.Col(), aAnchorPos.Row(), aAnchorPos.Tab(),
                            SC_REFTYPE_REF );
        bStarted = true;
    }
    else if (pViewData->IsAnyFillMode())
    {
        aAnchorPos.Set( nPosX, nPosY, nTab );
        bStarted = true;
    }
    else
    {
        // don't go there and back again
        if ( bStarted && pView->IsMarking( nPosX, nPosY, nTab ) )
        {
            // don't do anything
        }
        else
        {
            pView->DoneBlockMode( true );
            aAnchorPos.Set( nPosX, nPosY, nTab );
            ScMarkData& rMark = pViewData->GetMarkData();
            if ( rMark.IsMarked() || rMark.IsMultiMarked() )
            {
                pView->InitBlockMode( aAnchorPos.Col(), aAnchorPos.Row(),
                                        aAnchorPos.Tab(), true );
                bStarted = true;
            }
            else
                bStarted = false;
        }
    }
    bAnchor = true;
}

void ScViewFunctionSet::DestroyAnchor()
{
    bool bRefMode = SC_MOD()->IsFormulaMode();
    if (bRefMode)
        pViewData->GetView()->DoneRefMode( true );
    else
        pViewData->GetView()->DoneBlockMode( true );

    bAnchor = false;
}

void ScViewFunctionSet::SetAnchorFlag( bool bSet )
{
    bAnchor = bSet;
}

bool ScViewFunctionSet::SetCursorAtPoint( const Point& rPointPixel, bool /* bDontSelectAtCursor */ )
{
    if ( bDidSwitch )
    {
        if ( rPointPixel == aSwitchPos )
            return false;                   // don't scroll in wrong window
        else
            bDidSwitch = false;
    }
    aSwitchPos = rPointPixel;       // only important, if bDidSwitch

    //  treat position 0 as -1, so scrolling is always possible
    //  (with full screen and hidden headers, the top left border may be at 0)
    //  (moved from ScViewData::GetPosFromPixel)

    Point aEffPos = rPointPixel;
    if ( aEffPos.X() == 0 )
        aEffPos.X() = -1;
    if ( aEffPos.Y() == 0 )
        aEffPos.Y() = -1;

    //  Scrolling
    Size aWinSize = pEngine->GetWindow()->GetOutputSizePixel();
    bool bRightScroll  = ( aEffPos.X() >= aWinSize.Width() );
    bool bLeftScroll  = ( aEffPos.X() < 0 );
    bool bBottomScroll = ( aEffPos.Y() >= aWinSize.Height() );
    bool bTopScroll = ( aEffPos.Y() < 0 );
    bool bScroll = bRightScroll || bBottomScroll || bLeftScroll || bTopScroll;

    SCsCOL  nPosX;
    SCsROW  nPosY;
    pViewData->GetPosFromPixel( aEffPos.X(), aEffPos.Y(), GetWhich(),
                                nPosX, nPosY, true, true );     // with Repair

    // for Autofill switch in the center of cell
    // thereby don't prevent scrolling to bottom/right
    if ( pViewData->IsFillMode() || pViewData->GetFillMode() == SC_FILL_MATRIX )
    {
        bool bLeft, bTop;
        pViewData->GetMouseQuadrant( aEffPos, GetWhich(), nPosX, nPosY, bLeft, bTop );
        ScDocument* pDoc = pViewData->GetDocument();
        SCTAB nTab = pViewData->GetTabNo();
        if ( bLeft && !bRightScroll )
            do --nPosX; while ( nPosX>=0 && pDoc->ColHidden( nPosX, nTab ) );
        if ( bTop && !bBottomScroll )
        {
            if (--nPosY >= 0)
            {
                nPosY = pDoc->LastVisibleRow(0, nPosY, nTab);
                if (!ValidRow(nPosY))
                    nPosY = -1;
            }
        }
        // negative value is allowed
    }

    // moved out of fix limit?
    ScSplitPos eWhich = GetWhich();
    if ( eWhich == pViewData->GetActivePart() )
    {
        if ( pViewData->GetHSplitMode() == SC_SPLIT_FIX )
            if ( aEffPos.X() >= aWinSize.Width() )
            {
                if ( eWhich == SC_SPLIT_TOPLEFT )
                    pViewData->GetView()->ActivatePart( SC_SPLIT_TOPRIGHT ), bScroll = false, bDidSwitch = true;
                else if ( eWhich == SC_SPLIT_BOTTOMLEFT )
                    pViewData->GetView()->ActivatePart( SC_SPLIT_BOTTOMRIGHT ), bScroll = false, bDidSwitch = true;
            }

        if ( pViewData->GetVSplitMode() == SC_SPLIT_FIX )
            if ( aEffPos.Y() >= aWinSize.Height() )
            {
                if ( eWhich == SC_SPLIT_TOPLEFT )
                    pViewData->GetView()->ActivatePart( SC_SPLIT_BOTTOMLEFT ), bScroll = false, bDidSwitch = true;
                else if ( eWhich == SC_SPLIT_TOPRIGHT )
                    pViewData->GetView()->ActivatePart( SC_SPLIT_BOTTOMRIGHT ), bScroll = false, bDidSwitch = true;
            }
    }

    if (bScroll)
    {
        // Adjust update interval based on how far the mouse pointer is from the edge.
        sal_uLong nUpdateInterval = CalcUpdateInterval(
            aWinSize, aEffPos, bLeftScroll, bTopScroll, bRightScroll, bBottomScroll);
        pEngine->SetUpdateInterval(nUpdateInterval);
    }
    else
    {
        // Don't forget to reset the interval when not scrolling!
        pEngine->SetUpdateInterval(SELENG_AUTOREPEAT_INTERVAL);
    }

    pViewData->ResetOldCursor();
    return SetCursorAtCell( nPosX, nPosY, bScroll );
}

bool ScViewFunctionSet::SetCursorAtCell( SCsCOL nPosX, SCsROW nPosY, bool bScroll )
{
    ScTabView* pView = pViewData->GetView();
    SCTAB nTab = pViewData->GetTabNo();
    ScDocument* pDoc = pViewData->GetDocument();

    if ( pDoc->IsTabProtected(nTab) )
    {
        if (nPosX < 0 || nPosY < 0)
            return false;

        ScTableProtection* pProtect = pDoc->GetTabProtection(nTab);
        if (!pProtect)
            return false;

        bool bSkipProtected   = !pProtect->isOptionEnabled(ScTableProtection::SELECT_LOCKED_CELLS);
        bool bSkipUnprotected = !pProtect->isOptionEnabled(ScTableProtection::SELECT_UNLOCKED_CELLS);

        if ( bSkipProtected && bSkipUnprotected )
            return false;

        bool bCellProtected = pDoc->HasAttrib(nPosX, nPosY, nTab, nPosX, nPosY, nTab, HASATTR_PROTECTED);
        if ( (bCellProtected && bSkipProtected) || (!bCellProtected && bSkipUnprotected) )
            // Don't select this cell!
            return false;
    }

    ScModule* pScMod = SC_MOD();
    ScTabViewShell* pViewShell = pViewData->GetViewShell();
    bool bRefMode = pViewShell && pViewShell->IsRefInputMode();

    bool bHide = !bRefMode && !pViewData->IsAnyFillMode() &&
            ( nPosX != (SCsCOL) pViewData->GetCurX() || nPosY != (SCsROW) pViewData->GetCurY() );

    if (bHide)
        pView->HideAllCursors();

    if (bScroll)
    {
        if (bRefMode)
        {
            ScSplitPos eWhich = GetWhich();
            pView->AlignToCursor( nPosX, nPosY, SC_FOLLOW_LINE, &eWhich );
        }
        else
            pView->AlignToCursor( nPosX, nPosY, SC_FOLLOW_LINE );
    }

    if (bRefMode)
    {
        // if no input is possible from this doc, don't move the reference cursor around
        if ( !pScMod->IsModalMode(pViewData->GetSfxDocShell()) )
        {
            if (!bAnchor)
            {
                pView->DoneRefMode( true );
                pView->InitRefMode( nPosX, nPosY, pViewData->GetTabNo(), SC_REFTYPE_REF );
            }

            pView->UpdateRef( nPosX, nPosY, pViewData->GetTabNo() );
            pView->SelectionChanged();
        }
    }
    else if (pViewData->IsFillMode() ||
            (pViewData->GetFillMode() == SC_FILL_MATRIX && (nScFillModeMouseModifier & KEY_MOD1) ))
    {
        // If a matrix got touched, switch back to Autofill is possible with Ctrl

        SCCOL nStartX, nEndX;
        SCROW nStartY, nEndY; // Block
        SCTAB nDummy;
        pViewData->GetSimpleArea( nStartX, nStartY, nDummy, nEndX, nEndY, nDummy );

        if (pViewData->GetRefType() != SC_REFTYPE_FILL)
        {
            pView->InitRefMode( nStartX, nStartY, nTab, SC_REFTYPE_FILL );
            CreateAnchor();
        }

        ScRange aDelRange;
        bool bOldDelMark = pViewData->GetDelMark( aDelRange );

        if ( nPosX+1 >= (SCsCOL) nStartX && nPosX <= (SCsCOL) nEndX &&
             nPosY+1 >= (SCsROW) nStartY && nPosY <= (SCsROW) nEndY &&
             ( nPosX != nEndX || nPosY != nEndY ) )                     // minimize?
        {
            // direction (left or top)

            long nSizeX = 0;
            for (SCCOL i=nPosX+1; i<=nEndX; i++)
                nSizeX += pDoc->GetColWidth( i, nTab );
            long nSizeY = (long) pDoc->GetRowHeight( nPosY+1, nEndY, nTab );

            SCCOL nDelStartX = nStartX;
            SCROW nDelStartY = nStartY;
            if ( nSizeX > nSizeY )
                nDelStartX = nPosX + 1;
            else
                nDelStartY = nPosY + 1;
            // there is no need to check for zero, because nPosX/Y is also negative

            if ( nDelStartX < nStartX )
                nDelStartX = nStartX;
            if ( nDelStartY < nStartY )
                nDelStartY = nStartY;

            // set range

            pViewData->SetDelMark( ScRange( nDelStartX,nDelStartY,nTab,
                                            nEndX,nEndY,nTab ) );
            pViewData->GetView()->UpdateShrinkOverlay();

            pViewData->GetView()->
                PaintArea( nStartX,nDelStartY, nEndX,nEndY, SC_UPDATE_MARKS );

            nPosX = nEndX;      // keep red border around range
            nPosY = nEndY;

            // reference the right way up, if it's upside down below
            if ( nStartX != pViewData->GetRefStartX() || nStartY != pViewData->GetRefStartY() )
            {
                pViewData->GetView()->DoneRefMode();
                pViewData->GetView()->InitRefMode( nStartX, nStartY, nTab, SC_REFTYPE_FILL );
            }
        }
        else
        {
            if ( bOldDelMark )
            {
                pViewData->ResetDelMark();
                pViewData->GetView()->UpdateShrinkOverlay();
            }

            bool bNegX = ( nPosX < (SCsCOL) nStartX );
            bool bNegY = ( nPosY < (SCsROW) nStartY );

            long nSizeX = 0;
            if ( bNegX )
            {
                //  in SetCursorAtPoint hidden columns are skipped.
                //  They must be skipped here too, or the result will always be the first hidden column.
                do ++nPosX; while ( nPosX<nStartX && pDoc->ColHidden(nPosX, nTab) );
                for (SCCOL i=nPosX; i<nStartX; i++)
                    nSizeX += pDoc->GetColWidth( i, nTab );
            }
            else
                for (SCCOL i=nEndX+1; i<=nPosX; i++)
                    nSizeX += pDoc->GetColWidth( i, nTab );

            long nSizeY = 0;
            if ( bNegY )
            {
                //  in SetCursorAtPoint hidden rows are skipped.
                //  They must be skipped here too, or the result will always be the first hidden row.
                if (++nPosY < nStartY)
                {
                    nPosY = pDoc->FirstVisibleRow(nPosY, nStartY-1, nTab);
                    if (!ValidRow(nPosY))
                        nPosY = nStartY;
                }
                nSizeY += pDoc->GetRowHeight( nPosY, nStartY-1, nTab );
            }
            else
                nSizeY += pDoc->GetRowHeight( nEndY+1, nPosY, nTab );

            if ( nSizeX > nSizeY )          // Fill only ever in one direction
            {
                nPosY = nEndY;
                bNegY = false;
            }
            else
            {
                nPosX = nEndX;
                bNegX = false;
            }

            SCCOL nRefStX = bNegX ? nEndX : nStartX;
            SCROW nRefStY = bNegY ? nEndY : nStartY;
            if ( nRefStX != pViewData->GetRefStartX() || nRefStY != pViewData->GetRefStartY() )
            {
                pViewData->GetView()->DoneRefMode();
                pViewData->GetView()->InitRefMode( nRefStX, nRefStY, nTab, SC_REFTYPE_FILL );
            }
        }

        pView->UpdateRef( nPosX, nPosY, nTab );
    }
    else if (pViewData->IsAnyFillMode())
    {
        sal_uInt8 nMode = pViewData->GetFillMode();
        if ( nMode == SC_FILL_EMBED_LT || nMode == SC_FILL_EMBED_RB )
        {
            OSL_ENSURE( pDoc->IsEmbedded(), "!pDoc->IsEmbedded()" );
            ScRange aRange;
            pDoc->GetEmbedded( aRange);
            ScRefType eRefMode = (nMode == SC_FILL_EMBED_LT) ? SC_REFTYPE_EMBED_LT : SC_REFTYPE_EMBED_RB;
            if (pViewData->GetRefType() != eRefMode)
            {
                if ( nMode == SC_FILL_EMBED_LT )
                    pView->InitRefMode( aRange.aEnd.Col(), aRange.aEnd.Row(), nTab, eRefMode );
                else
                    pView->InitRefMode( aRange.aStart.Col(), aRange.aStart.Row(), nTab, eRefMode );
                CreateAnchor();
            }

            pView->UpdateRef( nPosX, nPosY, nTab );
        }
        else if ( nMode == SC_FILL_MATRIX )
        {
            SCCOL nStartX, nEndX;
            SCROW nStartY, nEndY; // Block
            SCTAB nDummy;
            pViewData->GetSimpleArea( nStartX, nStartY, nDummy, nEndX, nEndY, nDummy );

            if (pViewData->GetRefType() != SC_REFTYPE_FILL)
            {
                pView->InitRefMode( nStartX, nStartY, nTab, SC_REFTYPE_FILL );
                CreateAnchor();
            }

            if ( nPosX < nStartX ) nPosX = nStartX;
            if ( nPosY < nStartY ) nPosY = nStartY;

            pView->UpdateRef( nPosX, nPosY, nTab );
        }
        // else new modes
    }
    else                    // regular selection
    {
        bool bHideCur = bAnchor && ( (SCCOL)nPosX != pViewData->GetCurX() ||
                                     (SCROW)nPosY != pViewData->GetCurY() );
        if (bHideCur)
            pView->HideAllCursors();            // otherwise twice: Block and SetCursor

        if (bAnchor)
        {
            if (!bStarted)
            {
                bool bMove = ( nPosX != (SCsCOL) aAnchorPos.Col() ||
                                nPosY != (SCsROW) aAnchorPos.Row() );
                if ( bMove || ( pEngine && pEngine->GetMouseEvent().IsShift() ) )
                {
                    pView->InitBlockMode( aAnchorPos.Col(), aAnchorPos.Row(),
                                            aAnchorPos.Tab(), true );
                    bStarted = true;
                }
            }
            if (bStarted)
                // If the selection is already started, don't set the cursor.
                pView->MarkCursor( (SCCOL) nPosX, (SCROW) nPosY, nTab, false, false, true );
            else
                pView->SetCursor( (SCCOL) nPosX, (SCROW) nPosY );
        }
        else
        {
            ScMarkData& rMark = pViewData->GetMarkData();
            if (rMark.IsMarked() || rMark.IsMultiMarked())
            {
                pView->DoneBlockMode(true);
                pView->InitBlockMode( nPosX, nPosY, nTab, true );
                pView->MarkCursor( (SCCOL) nPosX, (SCROW) nPosY, nTab );

                aAnchorPos.Set( nPosX, nPosY, nTab );
                bStarted = true;
            }
            // #i3875# *Hack* When a new cell is Ctrl-clicked with no pre-selected cells,
            // it highlights that new cell as well as the old cell where the cursor is
            // positioned prior to the click.  A selection mode via Shift-F8 should also
            // follow the same behavior.
            else if ( pViewData->IsSelCtrlMouseClick() )
            {
                SCCOL nOldX = pViewData->GetCurX();
                SCROW nOldY = pViewData->GetCurY();

                pView->InitBlockMode( nOldX, nOldY, nTab, true );
                pView->MarkCursor( (SCCOL) nOldX, (SCROW) nOldY, nTab );

                if ( nOldX != nPosX || nOldY != nPosY )
                {
                    pView->DoneBlockMode( true );
                    pView->InitBlockMode( nPosX, nPosY, nTab, true );
                    pView->MarkCursor( (SCCOL) nPosX, (SCROW) nPosY, nTab );
                    aAnchorPos.Set( nPosX, nPosY, nTab );
                }

                bStarted = true;
            }
            pView->SetCursor( (SCCOL) nPosX, (SCROW) nPosY );
        }

        pViewData->SetRefStart( nPosX, nPosY, nTab );
        if (bHideCur)
            pView->ShowAllCursors();
    }

    if (bHide)
        pView->ShowAllCursors();

    return true;
}

bool ScViewFunctionSet::IsSelectionAtPoint( const Point& rPointPixel )
{
    bool bRefMode = SC_MOD()->IsFormulaMode();
    if (bRefMode)
        return false;

    if (pViewData->IsAnyFillMode())
        return false;

    ScMarkData& rMark = pViewData->GetMarkData();
    if (bAnchor || !rMark.IsMultiMarked())
    {
        SCsCOL  nPosX;
        SCsROW  nPosY;
        pViewData->GetPosFromPixel( rPointPixel.X(), rPointPixel.Y(), GetWhich(), nPosX, nPosY );
        return pViewData->GetMarkData().IsCellMarked( (SCCOL) nPosX, (SCROW) nPosY );
    }

    return false;
}

void ScViewFunctionSet::DeselectAtPoint( const Point& /* rPointPixel */ )
{
    // doesn't exist
}

void ScViewFunctionSet::DeselectAll()
{
    if (pViewData->IsAnyFillMode())
        return;

    bool bRefMode = SC_MOD()->IsFormulaMode();
    if (bRefMode)
    {
        pViewData->GetView()->DoneRefMode( false );
    }
    else
    {
        pViewData->GetView()->DoneBlockMode( false );
        pViewData->GetViewShell()->UpdateInputHandler();
    }

    bAnchor = false;
}

ScViewSelectionEngine::ScViewSelectionEngine( Window* pWindow, ScTabView* pView,
                                                ScSplitPos eSplitPos ) :
        SelectionEngine( pWindow, &pView->GetFunctionSet() ),
        eWhich( eSplitPos )
{
    SetSelectionMode( MULTIPLE_SELECTION );
    EnableDrag( true );
}

// column and row headers
ScHeaderFunctionSet::ScHeaderFunctionSet( ScViewData* pNewViewData ) :
        pViewData( pNewViewData ),
        bColumn( false ),
        eWhich( SC_SPLIT_TOPLEFT ),
        bAnchor( false ),
        nCursorPos( 0 )
{
    OSL_ENSURE(pViewData, "ViewData==0 at FunctionSet");
}

void ScHeaderFunctionSet::SetColumn( bool bSet )
{
    bColumn = bSet;
}

void ScHeaderFunctionSet::SetWhich( ScSplitPos eNew )
{
    eWhich = eNew;
}

void ScHeaderFunctionSet::BeginDrag()
{
    // doesn't exist
}

void ScHeaderFunctionSet::CreateAnchor()
{
    if (bAnchor)
        return;

    ScTabView* pView = pViewData->GetView();
    pView->DoneBlockMode( true );
    if (bColumn)
    {
        pView->InitBlockMode( static_cast<SCCOL>(nCursorPos), 0, pViewData->GetTabNo(), true, true, false );
        pView->MarkCursor( static_cast<SCCOL>(nCursorPos), MAXROW, pViewData->GetTabNo() );
    }
    else
    {
        pView->InitBlockMode( 0, nCursorPos, pViewData->GetTabNo(), true, false, true );
        pView->MarkCursor( MAXCOL, nCursorPos, pViewData->GetTabNo() );
    }
    bAnchor = true;
}

void ScHeaderFunctionSet::DestroyAnchor()
{
    pViewData->GetView()->DoneBlockMode( true );
    bAnchor = false;
}

bool ScHeaderFunctionSet::SetCursorAtPoint( const Point& rPointPixel, bool /* bDontSelectAtCursor */ )
{
    if ( bDidSwitch )
    {
        // next valid position has to be originated from another window
        if ( rPointPixel == aSwitchPos )
            return false;                   // don't scroll in the wrong window
        else
            bDidSwitch = false;
    }

    //  Scrolling
    Size aWinSize = pViewData->GetActiveWin()->GetOutputSizePixel();
    bool bScroll;
    if (bColumn)
        bScroll = ( rPointPixel.X() < 0 || rPointPixel.X() >= aWinSize.Width() );
    else
        bScroll = ( rPointPixel.Y() < 0 || rPointPixel.Y() >= aWinSize.Height() );

    // moved out of fix limit?
    bool bSwitched = false;
    if ( bColumn )
    {
        if ( pViewData->GetHSplitMode() == SC_SPLIT_FIX )
        {
            if ( rPointPixel.X() > aWinSize.Width() )
            {
                if ( eWhich == SC_SPLIT_TOPLEFT )
                    pViewData->GetView()->ActivatePart( SC_SPLIT_TOPRIGHT ), bSwitched = true;
                else if ( eWhich == SC_SPLIT_BOTTOMLEFT )
                    pViewData->GetView()->ActivatePart( SC_SPLIT_BOTTOMRIGHT ), bSwitched = true;
            }
        }
    }
    else                // column headers
    {
        if ( pViewData->GetVSplitMode() == SC_SPLIT_FIX )
        {
            if ( rPointPixel.Y() > aWinSize.Height() )
            {
                if ( eWhich == SC_SPLIT_TOPLEFT )
                    pViewData->GetView()->ActivatePart( SC_SPLIT_BOTTOMLEFT ), bSwitched = true;
                else if ( eWhich == SC_SPLIT_TOPRIGHT )
                    pViewData->GetView()->ActivatePart( SC_SPLIT_BOTTOMRIGHT ), bSwitched = true;
            }
        }
    }
    if (bSwitched)
    {
        aSwitchPos = rPointPixel;
        bDidSwitch = true;
        return false;               // do not crunch with wrong positions
    }

    SCsCOL  nPosX;
    SCsROW  nPosY;
    pViewData->GetPosFromPixel( rPointPixel.X(), rPointPixel.Y(), pViewData->GetActivePart(),
                                nPosX, nPosY, false );
    if (bColumn)
    {
        nCursorPos = static_cast<SCCOLROW>(nPosX);
        nPosY = pViewData->GetPosY(WhichV(pViewData->GetActivePart()));
    }
    else
    {
        nCursorPos = static_cast<SCCOLROW>(nPosY);
        nPosX = pViewData->GetPosX(WhichH(pViewData->GetActivePart()));
    }

    ScTabView* pView = pViewData->GetView();
    bool bHide = pViewData->GetCurX() != nPosX ||
                 pViewData->GetCurY() != nPosY;
    if (bHide)
        pView->HideAllCursors();

    if (bScroll)
        pView->AlignToCursor( nPosX, nPosY, SC_FOLLOW_LINE );
    pView->SetCursor( nPosX, nPosY );

    if ( !bAnchor || !pView->IsBlockMode() )
    {
        pView->DoneBlockMode( true );
        pViewData->GetMarkData().MarkToMulti();         //! who changes this?
        pView->InitBlockMode( nPosX, nPosY, pViewData->GetTabNo(), true, bColumn, !bColumn );

        bAnchor = true;
    }

    pView->MarkCursor( nPosX, nPosY, pViewData->GetTabNo(), bColumn, !bColumn );

    // SelectionChanged inside of HideCursor because of UpdateAutoFillMark
    pView->SelectionChanged();

    if (bHide)
        pView->ShowAllCursors();

    return true;
}

bool ScHeaderFunctionSet::IsSelectionAtPoint( const Point& rPointPixel )
{
    SCsCOL  nPosX;
    SCsROW  nPosY;
    pViewData->GetPosFromPixel( rPointPixel.X(), rPointPixel.Y(), pViewData->GetActivePart(),
                                nPosX, nPosY, false );

    ScMarkData& rMark = pViewData->GetMarkData();
    if (bColumn)
        return rMark.IsColumnMarked( nPosX );
    else
        return rMark.IsRowMarked( nPosY );
}

void ScHeaderFunctionSet::DeselectAtPoint( const Point& /* rPointPixel */ )
{
}

void ScHeaderFunctionSet::DeselectAll()
{
    pViewData->GetView()->DoneBlockMode( false );
    bAnchor = false;
}

ScHeaderSelectionEngine::ScHeaderSelectionEngine( Window* pWindow, ScHeaderFunctionSet* pFuncSet ) :
        SelectionEngine( pWindow, pFuncSet )
{
    SetSelectionMode( MULTIPLE_SELECTION );
    EnableDrag( false );
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
