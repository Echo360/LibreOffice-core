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

#include <vcl/window.hxx>
#include <vcl/taskpanelist.hxx>

// declare system types in sysdata.hxx
#include <vcl/sysdata.hxx>

#include <salframe.hxx>
#include <salobj.hxx>
#include <salgdi.hxx>
#include <svdata.hxx>
#include <window.h>
#include <brdwin.hxx>
#include <helpwin.hxx>

#include <com/sun/star/awt/XTopWindow.hpp>

#include <set>
#include <typeinfo>

using namespace ::com::sun::star::uno;
using namespace ::com::sun::star::lang;
using namespace ::com::sun::star::datatransfer::clipboard;
using namespace ::com::sun::star::datatransfer::dnd;
using namespace ::com::sun::star;

using ::com::sun::star::awt::XTopWindow;

struct ImplCalcToTopData
{
    ImplCalcToTopData*  mpNext;
    Window*             mpWindow;
    Region*             mpInvalidateRegion;
};

Window* Window::ImplGetTopmostFrameWindow()
{
    Window *pTopmostParent = this;
    while( pTopmostParent->ImplGetParent() )
        pTopmostParent = pTopmostParent->ImplGetParent();
    return pTopmostParent->mpWindowImpl->mpFrameWindow;
}

void Window::ImplInsertWindow( Window* pParent )
{
    mpWindowImpl->mpParent            = pParent;
    mpWindowImpl->mpRealParent        = pParent;

    if ( pParent && !mpWindowImpl->mbFrame )
    {
        // search frame window and set window frame data
        Window* pFrameParent = pParent->mpWindowImpl->mpFrameWindow;
        mpWindowImpl->mpFrameData     = pFrameParent->mpWindowImpl->mpFrameData;
        mpWindowImpl->mpFrame         = pFrameParent->mpWindowImpl->mpFrame;
        mpWindowImpl->mpFrameWindow   = pFrameParent;
        mpWindowImpl->mbFrame         = false;

        // search overlap window and insert window in list
        if ( ImplIsOverlapWindow() )
        {
            Window* pFirstOverlapParent = pParent;
            while ( !pFirstOverlapParent->ImplIsOverlapWindow() )
                pFirstOverlapParent = pFirstOverlapParent->ImplGetParent();
            mpWindowImpl->mpOverlapWindow = pFirstOverlapParent;

            mpWindowImpl->mpNextOverlap = mpWindowImpl->mpFrameData->mpFirstOverlap;
            mpWindowImpl->mpFrameData->mpFirstOverlap = this;

            // Overlap-Windows are by default the uppermost
            mpWindowImpl->mpNext = pFirstOverlapParent->mpWindowImpl->mpFirstOverlap;
            pFirstOverlapParent->mpWindowImpl->mpFirstOverlap = this;
            if ( !pFirstOverlapParent->mpWindowImpl->mpLastOverlap )
                pFirstOverlapParent->mpWindowImpl->mpLastOverlap = this;
            else
                mpWindowImpl->mpNext->mpWindowImpl->mpPrev = this;
        }
        else
        {
            if ( pParent->ImplIsOverlapWindow() )
                mpWindowImpl->mpOverlapWindow = pParent;
            else
                mpWindowImpl->mpOverlapWindow = pParent->mpWindowImpl->mpOverlapWindow;
            mpWindowImpl->mpPrev = pParent->mpWindowImpl->mpLastChild;
            pParent->mpWindowImpl->mpLastChild = this;
            if ( !pParent->mpWindowImpl->mpFirstChild )
                pParent->mpWindowImpl->mpFirstChild = this;
            else
                mpWindowImpl->mpPrev->mpWindowImpl->mpNext = this;
        }
    }
}

void Window::ImplRemoveWindow( bool bRemoveFrameData )
{
    // remove window from the lists
    if ( !mpWindowImpl->mbFrame )
    {
        if ( ImplIsOverlapWindow() )
        {
            if ( mpWindowImpl->mpFrameData->mpFirstOverlap == this )
                mpWindowImpl->mpFrameData->mpFirstOverlap = mpWindowImpl->mpNextOverlap;
            else
            {
                Window* pTempWin = mpWindowImpl->mpFrameData->mpFirstOverlap;
                while ( pTempWin->mpWindowImpl->mpNextOverlap != this )
                    pTempWin = pTempWin->mpWindowImpl->mpNextOverlap;
                pTempWin->mpWindowImpl->mpNextOverlap = mpWindowImpl->mpNextOverlap;
            }

            if ( mpWindowImpl->mpPrev )
                mpWindowImpl->mpPrev->mpWindowImpl->mpNext = mpWindowImpl->mpNext;
            else
                mpWindowImpl->mpOverlapWindow->mpWindowImpl->mpFirstOverlap = mpWindowImpl->mpNext;
            if ( mpWindowImpl->mpNext )
                mpWindowImpl->mpNext->mpWindowImpl->mpPrev = mpWindowImpl->mpPrev;
            else
                mpWindowImpl->mpOverlapWindow->mpWindowImpl->mpLastOverlap = mpWindowImpl->mpPrev;
        }
        else
        {
            if ( mpWindowImpl->mpPrev )
                mpWindowImpl->mpPrev->mpWindowImpl->mpNext = mpWindowImpl->mpNext;
            else if ( mpWindowImpl->mpParent )
                mpWindowImpl->mpParent->mpWindowImpl->mpFirstChild = mpWindowImpl->mpNext;
            if ( mpWindowImpl->mpNext )
                mpWindowImpl->mpNext->mpWindowImpl->mpPrev = mpWindowImpl->mpPrev;
            else if ( mpWindowImpl->mpParent )
                mpWindowImpl->mpParent->mpWindowImpl->mpLastChild = mpWindowImpl->mpPrev;
        }

        mpWindowImpl->mpPrev = NULL;
        mpWindowImpl->mpNext = NULL;
    }

    if ( bRemoveFrameData )
    {
        // release the graphic
        OutputDevice *pOutDev = GetOutDev();
        pOutDev->ReleaseGraphics();
    }
}

void Window::reorderWithinParent(sal_uInt16 nNewPosition)
{
    sal_uInt16 nChildCount = 0;
    Window *pSource = mpWindowImpl->mpParent->mpWindowImpl->mpFirstChild;
    while (pSource)
    {
        if (nChildCount == nNewPosition)
            break;
        pSource = pSource->mpWindowImpl->mpNext;
        nChildCount++;
    }

    if (pSource == this) //already at the right place
        return;

    ImplRemoveWindow(false);

    if (pSource)
    {
        mpWindowImpl->mpNext = pSource;
        mpWindowImpl->mpPrev = pSource->mpWindowImpl->mpPrev;
        pSource->mpWindowImpl->mpPrev = this;
    }
    else
        mpWindowImpl->mpParent->mpWindowImpl->mpLastChild = this;

    if (mpWindowImpl->mpPrev)
        mpWindowImpl->mpPrev->mpWindowImpl->mpNext = this;
    else
        mpWindowImpl->mpParent->mpWindowImpl->mpFirstChild = this;
}

void Window::ImplToBottomChild()
{
    if ( !ImplIsOverlapWindow() && !mpWindowImpl->mbReallyVisible && (mpWindowImpl->mpParent->mpWindowImpl->mpLastChild != this) )
    {
        // put the window to the end of the list
        if ( mpWindowImpl->mpPrev )
            mpWindowImpl->mpPrev->mpWindowImpl->mpNext = mpWindowImpl->mpNext;
        else
            mpWindowImpl->mpParent->mpWindowImpl->mpFirstChild = mpWindowImpl->mpNext;
        mpWindowImpl->mpNext->mpWindowImpl->mpPrev = mpWindowImpl->mpPrev;
        mpWindowImpl->mpPrev = mpWindowImpl->mpParent->mpWindowImpl->mpLastChild;
        mpWindowImpl->mpParent->mpWindowImpl->mpLastChild = this;
        mpWindowImpl->mpPrev->mpWindowImpl->mpNext = this;
        mpWindowImpl->mpNext = NULL;
    }
}

void Window::ImplCalcToTop( ImplCalcToTopData* pPrevData )
{
    DBG_ASSERT( ImplIsOverlapWindow(), "Window::ImplCalcToTop(): Is not a OverlapWindow" );

    if ( !mpWindowImpl->mbFrame )
    {
        if ( IsReallyVisible() )
        {
            // calculate region, where the window overlaps with other windows
            Point aPoint( mnOutOffX, mnOutOffY );
            Region  aRegion( Rectangle( aPoint,
                                        Size( mnOutWidth, mnOutHeight ) ) );
            Region  aInvalidateRegion;
            ImplCalcOverlapRegionOverlaps( aRegion, aInvalidateRegion );

            if ( !aInvalidateRegion.IsEmpty() )
            {
                ImplCalcToTopData* pData    = new ImplCalcToTopData;
                pPrevData->mpNext           = pData;
                pData->mpNext               = NULL;
                pData->mpWindow             = this;
                pData->mpInvalidateRegion   = new Region( aInvalidateRegion );
            }
        }
    }
}

void Window::ImplToTop( sal_uInt16 nFlags )
{
    DBG_ASSERT( ImplIsOverlapWindow(), "Window::ImplToTop(): Is not a OverlapWindow" );

    if ( mpWindowImpl->mbFrame )
    {
        // on a mouse click in the external window, it is the latter's
        // responsibility to assure our frame is put in front
        if ( !mpWindowImpl->mpFrameData->mbHasFocus &&
             !mpWindowImpl->mpFrameData->mbSysObjFocus &&
             !mpWindowImpl->mpFrameData->mbInSysObjFocusHdl &&
             !mpWindowImpl->mpFrameData->mbInSysObjToTopHdl )
        {
            // do not bring floating windows on the client to top
            if( !ImplGetClientWindow() || !(ImplGetClientWindow()->GetStyle() & WB_SYSTEMFLOATWIN) )
            {
                sal_uInt16 nSysFlags = 0;
                if ( nFlags & TOTOP_RESTOREWHENMIN )
                    nSysFlags |= SAL_FRAME_TOTOP_RESTOREWHENMIN;
                if ( nFlags & TOTOP_FOREGROUNDTASK )
                    nSysFlags |= SAL_FRAME_TOTOP_FOREGROUNDTASK;
                if ( nFlags & TOTOP_GRABFOCUSONLY )
                    nSysFlags |= SAL_FRAME_TOTOP_GRABFOCUS_ONLY;
                mpWindowImpl->mpFrame->ToTop( nSysFlags );
            }
        }
    }
    else
    {
        if ( mpWindowImpl->mpOverlapWindow->mpWindowImpl->mpFirstOverlap != this )
        {
            // remove window from the list
            mpWindowImpl->mpPrev->mpWindowImpl->mpNext = mpWindowImpl->mpNext;
            if ( mpWindowImpl->mpNext )
                mpWindowImpl->mpNext->mpWindowImpl->mpPrev = mpWindowImpl->mpPrev;
            else
                mpWindowImpl->mpOverlapWindow->mpWindowImpl->mpLastOverlap = mpWindowImpl->mpPrev;

            // take AlwaysOnTop into account
            bool    bOnTop = IsAlwaysOnTopEnabled();
            Window* pNextWin = mpWindowImpl->mpOverlapWindow->mpWindowImpl->mpFirstOverlap;
            if ( !bOnTop )
            {
                while ( pNextWin )
                {
                    if ( !pNextWin->IsAlwaysOnTopEnabled() )
                        break;
                    pNextWin = pNextWin->mpWindowImpl->mpNext;
                }
            }

            // check TopLevel
            sal_uInt8 nTopLevel = mpWindowImpl->mpOverlapData->mnTopLevel;
            while ( pNextWin )
            {
                if ( (bOnTop != pNextWin->IsAlwaysOnTopEnabled()) ||
                     (nTopLevel <= pNextWin->mpWindowImpl->mpOverlapData->mnTopLevel) )
                    break;
                pNextWin = pNextWin->mpWindowImpl->mpNext;
            }

            // add the window to the list again
            mpWindowImpl->mpNext = pNextWin;
            if ( pNextWin )
            {
                mpWindowImpl->mpPrev = pNextWin->mpWindowImpl->mpPrev;
                pNextWin->mpWindowImpl->mpPrev = this;
            }
            else
            {
                mpWindowImpl->mpPrev = mpWindowImpl->mpOverlapWindow->mpWindowImpl->mpLastOverlap;
                mpWindowImpl->mpOverlapWindow->mpWindowImpl->mpLastOverlap = this;
            }
            if ( mpWindowImpl->mpPrev )
                mpWindowImpl->mpPrev->mpWindowImpl->mpNext = this;
            else
                mpWindowImpl->mpOverlapWindow->mpWindowImpl->mpFirstOverlap = this;

            // recalculate ClipRegion of this and all overlapping windows
            if ( IsReallyVisible() )
            {
                // reset background storage
                if ( mpWindowImpl->mpFrameData->mpFirstBackWin )
                    ImplInvalidateAllOverlapBackgrounds();
                mpWindowImpl->mpOverlapWindow->ImplSetClipFlagOverlapWindows();
            }
        }
    }
}

void Window::ImplStartToTop( sal_uInt16 nFlags )
{
    ImplCalcToTopData   aStartData;
    ImplCalcToTopData*  pCurData;
    ImplCalcToTopData*  pNextData;
    Window* pOverlapWindow;
    if ( ImplIsOverlapWindow() )
        pOverlapWindow = this;
    else
        pOverlapWindow = mpWindowImpl->mpOverlapWindow;

    // first calculate paint areas
    Window* pTempOverlapWindow = pOverlapWindow;
    aStartData.mpNext = NULL;
    pCurData = &aStartData;
    do
    {
        pTempOverlapWindow->ImplCalcToTop( pCurData );
        if ( pCurData->mpNext )
            pCurData = pCurData->mpNext;
        pTempOverlapWindow = pTempOverlapWindow->mpWindowImpl->mpOverlapWindow;
    }
    while ( !pTempOverlapWindow->mpWindowImpl->mbFrame );
    // next calculate the paint areas of the ChildOverlap windows
    pTempOverlapWindow = mpWindowImpl->mpFirstOverlap;
    while ( pTempOverlapWindow )
    {
        pTempOverlapWindow->ImplCalcToTop( pCurData );
        if ( pCurData->mpNext )
            pCurData = pCurData->mpNext;
        pTempOverlapWindow = pTempOverlapWindow->mpWindowImpl->mpNext;
    }

    // and next change the windows list
    pTempOverlapWindow = pOverlapWindow;
    do
    {
        pTempOverlapWindow->ImplToTop( nFlags );
        pTempOverlapWindow = pTempOverlapWindow->mpWindowImpl->mpOverlapWindow;
    }
    while ( !pTempOverlapWindow->mpWindowImpl->mbFrame );
    // as last step invalidate the invalid areas
    pCurData = aStartData.mpNext;
    while ( pCurData )
    {
        pCurData->mpWindow->ImplInvalidateFrameRegion( pCurData->mpInvalidateRegion, INVALIDATE_CHILDREN );
        pNextData = pCurData->mpNext;
        delete pCurData->mpInvalidateRegion;
        delete pCurData;
        pCurData = pNextData;
    }
}

void Window::ImplFocusToTop( sal_uInt16 nFlags, bool bReallyVisible )
{
    // do we need to fetch the focus?
    if ( !(nFlags & TOTOP_NOGRABFOCUS) )
    {
        // first window with GrabFocus-Activate gets the focus
        Window* pFocusWindow = this;
        while ( !pFocusWindow->ImplIsOverlapWindow() )
        {
            // if the window has no BorderWindow, we
            // should always find the belonging BorderWindow
            if ( !pFocusWindow->mpWindowImpl->mpBorderWindow )
            {
                if ( pFocusWindow->mpWindowImpl->mnActivateMode & ACTIVATE_MODE_GRABFOCUS )
                    break;
            }
            pFocusWindow = pFocusWindow->ImplGetParent();
        }
        if ( (pFocusWindow->mpWindowImpl->mnActivateMode & ACTIVATE_MODE_GRABFOCUS) &&
             !pFocusWindow->HasChildPathFocus( true ) )
            pFocusWindow->GrabFocus();
    }

    if ( bReallyVisible )
        ImplGenerateMouseMove();
}

void Window::ImplShowAllOverlaps()
{
    Window* pOverlapWindow = mpWindowImpl->mpFirstOverlap;
    while ( pOverlapWindow )
    {
        if ( pOverlapWindow->mpWindowImpl->mbOverlapVisible )
        {
            pOverlapWindow->Show( true, SHOW_NOACTIVATE );
            pOverlapWindow->mpWindowImpl->mbOverlapVisible = false;
        }

        pOverlapWindow = pOverlapWindow->mpWindowImpl->mpNext;
    }
}

void Window::ImplHideAllOverlaps()
{
    Window* pOverlapWindow = mpWindowImpl->mpFirstOverlap;
    while ( pOverlapWindow )
    {
        if ( pOverlapWindow->IsVisible() )
        {
            pOverlapWindow->mpWindowImpl->mbOverlapVisible = true;
            pOverlapWindow->Show( false );
        }

        pOverlapWindow = pOverlapWindow->mpWindowImpl->mpNext;
    }
}

void Window::ToTop( sal_uInt16 nFlags )
{

    ImplStartToTop( nFlags );
    ImplFocusToTop( nFlags, IsReallyVisible() );
}

void Window::SetZOrder( Window* pRefWindow, sal_uInt16 nFlags )
{

    if ( mpWindowImpl->mpBorderWindow )
    {
        mpWindowImpl->mpBorderWindow->SetZOrder( pRefWindow, nFlags );
        return;
    }

    if ( nFlags & WINDOW_ZORDER_FIRST )
    {
        if ( ImplIsOverlapWindow() )
            pRefWindow = mpWindowImpl->mpOverlapWindow->mpWindowImpl->mpFirstOverlap;
        else
            pRefWindow = mpWindowImpl->mpParent->mpWindowImpl->mpFirstChild;
        nFlags |= WINDOW_ZORDER_BEFOR;
    }
    else if ( nFlags & WINDOW_ZORDER_LAST )
    {
        if ( ImplIsOverlapWindow() )
            pRefWindow = mpWindowImpl->mpOverlapWindow->mpWindowImpl->mpLastOverlap;
        else
            pRefWindow = mpWindowImpl->mpParent->mpWindowImpl->mpLastChild;
        nFlags |= WINDOW_ZORDER_BEHIND;
    }

    while ( pRefWindow && pRefWindow->mpWindowImpl->mpBorderWindow )
        pRefWindow = pRefWindow->mpWindowImpl->mpBorderWindow;
    if (!pRefWindow || pRefWindow == this || mpWindowImpl->mbFrame)
        return;

    DBG_ASSERT( pRefWindow->mpWindowImpl->mpParent == mpWindowImpl->mpParent, "Window::SetZOrder() - pRefWindow has other parent" );
    if ( nFlags & WINDOW_ZORDER_BEFOR )
    {
        if ( pRefWindow->mpWindowImpl->mpPrev == this )
            return;

        if ( ImplIsOverlapWindow() )
        {
            if ( mpWindowImpl->mpPrev )
                mpWindowImpl->mpPrev->mpWindowImpl->mpNext = mpWindowImpl->mpNext;
            else
                mpWindowImpl->mpOverlapWindow->mpWindowImpl->mpFirstOverlap = mpWindowImpl->mpNext;
            if ( mpWindowImpl->mpNext )
                mpWindowImpl->mpNext->mpWindowImpl->mpPrev = mpWindowImpl->mpPrev;
            else
                mpWindowImpl->mpOverlapWindow->mpWindowImpl->mpLastOverlap = mpWindowImpl->mpPrev;
            if ( !pRefWindow->mpWindowImpl->mpPrev )
                mpWindowImpl->mpOverlapWindow->mpWindowImpl->mpFirstOverlap = this;
        }
        else
        {
            if ( mpWindowImpl->mpPrev )
                mpWindowImpl->mpPrev->mpWindowImpl->mpNext = mpWindowImpl->mpNext;
            else
                mpWindowImpl->mpParent->mpWindowImpl->mpFirstChild = mpWindowImpl->mpNext;
            if ( mpWindowImpl->mpNext )
                mpWindowImpl->mpNext->mpWindowImpl->mpPrev = mpWindowImpl->mpPrev;
            else
                mpWindowImpl->mpParent->mpWindowImpl->mpLastChild = mpWindowImpl->mpPrev;
            if ( !pRefWindow->mpWindowImpl->mpPrev )
                mpWindowImpl->mpParent->mpWindowImpl->mpFirstChild = this;
        }

        mpWindowImpl->mpPrev = pRefWindow->mpWindowImpl->mpPrev;
        mpWindowImpl->mpNext = pRefWindow;
        if ( mpWindowImpl->mpPrev )
            mpWindowImpl->mpPrev->mpWindowImpl->mpNext = this;
        mpWindowImpl->mpNext->mpWindowImpl->mpPrev = this;
    }
    else if ( nFlags & WINDOW_ZORDER_BEHIND )
    {
        if ( pRefWindow->mpWindowImpl->mpNext == this )
            return;

        if ( ImplIsOverlapWindow() )
        {
            if ( mpWindowImpl->mpPrev )
                mpWindowImpl->mpPrev->mpWindowImpl->mpNext = mpWindowImpl->mpNext;
            else
                mpWindowImpl->mpOverlapWindow->mpWindowImpl->mpFirstOverlap = mpWindowImpl->mpNext;
            if ( mpWindowImpl->mpNext )
                mpWindowImpl->mpNext->mpWindowImpl->mpPrev = mpWindowImpl->mpPrev;
            else
                mpWindowImpl->mpOverlapWindow->mpWindowImpl->mpLastOverlap = mpWindowImpl->mpPrev;
            if ( !pRefWindow->mpWindowImpl->mpNext )
                mpWindowImpl->mpOverlapWindow->mpWindowImpl->mpLastOverlap = this;
        }
        else
        {
            if ( mpWindowImpl->mpPrev )
                mpWindowImpl->mpPrev->mpWindowImpl->mpNext = mpWindowImpl->mpNext;
            else
                mpWindowImpl->mpParent->mpWindowImpl->mpFirstChild = mpWindowImpl->mpNext;
            if ( mpWindowImpl->mpNext )
                mpWindowImpl->mpNext->mpWindowImpl->mpPrev = mpWindowImpl->mpPrev;
            else
                mpWindowImpl->mpParent->mpWindowImpl->mpLastChild = mpWindowImpl->mpPrev;
            if ( !pRefWindow->mpWindowImpl->mpNext )
                mpWindowImpl->mpParent->mpWindowImpl->mpLastChild = this;
        }

        mpWindowImpl->mpPrev = pRefWindow;
        mpWindowImpl->mpNext = pRefWindow->mpWindowImpl->mpNext;
        if ( mpWindowImpl->mpNext )
            mpWindowImpl->mpNext->mpWindowImpl->mpPrev = this;
        mpWindowImpl->mpPrev->mpWindowImpl->mpNext = this;
    }

    if ( IsReallyVisible() )
    {
        // restore background storage
        if ( mpWindowImpl->mpFrameData->mpFirstBackWin )
            ImplInvalidateAllOverlapBackgrounds();

        if ( mpWindowImpl->mbInitWinClipRegion || !mpWindowImpl->maWinClipRegion.IsEmpty() )
        {
            bool bInitWinClipRegion = mpWindowImpl->mbInitWinClipRegion;
            ImplSetClipFlag();

            // When ClipRegion was not initialised, assume
            // the window has not been sent, therefore do not
            // trigger any Invalidates. This is an optimization
            // for HTML documents with many controls. If this
            // check gives problems, a flag should be introduced
            // which tracks whether the window has already been
            // emitted after Show
            if ( !bInitWinClipRegion )
            {
                // Invalidate all windows which are next to each other
                // Is INCOMPLETE !!!
                Rectangle   aWinRect( Point( mnOutOffX, mnOutOffY ), Size( mnOutWidth, mnOutHeight ) );
                Window*     pWindow = NULL;
                if ( ImplIsOverlapWindow() )
                {
                    if ( mpWindowImpl->mpOverlapWindow )
                        pWindow = mpWindowImpl->mpOverlapWindow->mpWindowImpl->mpFirstOverlap;
                }
                else
                    pWindow = ImplGetParent()->mpWindowImpl->mpFirstChild;
                // Invalidate all windows in front of us and which are covered by us
                while ( pWindow )
                {
                    if ( pWindow == this )
                        break;
                    Rectangle aCompRect( Point( pWindow->mnOutOffX, pWindow->mnOutOffY ),
                                         Size( pWindow->mnOutWidth, pWindow->mnOutHeight ) );
                    if ( aWinRect.IsOver( aCompRect ) )
                        pWindow->Invalidate( INVALIDATE_CHILDREN | INVALIDATE_NOTRANSPARENT );
                    pWindow = pWindow->mpWindowImpl->mpNext;
                }

                // If we are covered by a window in the background
                // we should redraw it
                while ( pWindow )
                {
                    if ( pWindow != this )
                    {
                        Rectangle aCompRect( Point( pWindow->mnOutOffX, pWindow->mnOutOffY ),
                                             Size( pWindow->mnOutWidth, pWindow->mnOutHeight ) );
                        if ( aWinRect.IsOver( aCompRect ) )
                        {
                            Invalidate( INVALIDATE_CHILDREN | INVALIDATE_NOTRANSPARENT );
                            break;
                        }
                    }
                    pWindow = pWindow->mpWindowImpl->mpNext;
                }
            }
        }
    }
}

void Window::EnableAlwaysOnTop( bool bEnable )
{

    mpWindowImpl->mbAlwaysOnTop = bEnable;

    if ( mpWindowImpl->mpBorderWindow )
        mpWindowImpl->mpBorderWindow->EnableAlwaysOnTop( bEnable );
    else if ( bEnable && IsReallyVisible() )
        ToTop();

    if ( mpWindowImpl->mbFrame )
        mpWindowImpl->mpFrame->SetAlwaysOnTop( bEnable );
}

bool Window::IsTopWindow() const
{
    if ( mpWindowImpl->mbInDtor )
        return false;

    // topwindows must be frames or they must have a borderwindow which is a frame
    if( !mpWindowImpl->mbFrame && (!mpWindowImpl->mpBorderWindow || (mpWindowImpl->mpBorderWindow && !mpWindowImpl->mpBorderWindow->mpWindowImpl->mbFrame) ) )
        return false;

    ImplGetWinData();
    if( mpWindowImpl->mpWinData->mnIsTopWindow == (sal_uInt16)~0)    // still uninitialized
    {
        // #113722#, cache result of expensive queryInterface call
        Window *pThisWin = (Window*)this;
        uno::Reference< XTopWindow > xTopWindow( pThisWin->GetComponentInterface(), UNO_QUERY );
        pThisWin->mpWindowImpl->mpWinData->mnIsTopWindow = xTopWindow.is() ? 1 : 0;
    }
    return mpWindowImpl->mpWinData->mnIsTopWindow == 1 ? true : false;
}

Window* Window::FindWindow( const Point& rPos ) const
{

    Point aPos = OutputToScreenPixel( rPos );
    return ((Window*)this)->ImplFindWindow( aPos );
}

Window* Window::ImplFindWindow( const Point& rFramePos )
{
    Window* pTempWindow;
    Window* pFindWindow;

    // first check all overlapping windows
    pTempWindow = mpWindowImpl->mpFirstOverlap;
    while ( pTempWindow )
    {
        pFindWindow = pTempWindow->ImplFindWindow( rFramePos );
        if ( pFindWindow )
            return pFindWindow;
        pTempWindow = pTempWindow->mpWindowImpl->mpNext;
    }

    // then we check our window
    if ( !mpWindowImpl->mbVisible )
        return NULL;

    sal_uInt16 nHitTest = ImplHitTest( rFramePos );
    if ( nHitTest & WINDOW_HITTEST_INSIDE )
    {
        // and then we check all child windows
        pTempWindow = mpWindowImpl->mpFirstChild;
        while ( pTempWindow )
        {
            pFindWindow = pTempWindow->ImplFindWindow( rFramePos );
            if ( pFindWindow )
                return pFindWindow;
            pTempWindow = pTempWindow->mpWindowImpl->mpNext;
        }

        if ( nHitTest & WINDOW_HITTEST_TRANSPARENT )
            return NULL;
        else
            return this;
    }

    return NULL;
}

bool Window::ImplIsRealParentPath( const Window* pWindow ) const
{
    pWindow = pWindow->GetParent();
    while ( pWindow )
    {
        if ( pWindow == this )
            return true;
        pWindow = pWindow->GetParent();
    }

    return false;
}

bool Window::ImplIsChild( const Window* pWindow, bool bSystemWindow ) const
{
    do
    {
        if ( !bSystemWindow && pWindow->ImplIsOverlapWindow() )
            break;

        pWindow = pWindow->ImplGetParent();

        if ( pWindow == this )
            return true;
    }
    while ( pWindow );

    return false;
}

bool Window::ImplIsWindowOrChild( const Window* pWindow, bool bSystemWindow ) const
{
    if ( this == pWindow )
        return true;
    return ImplIsChild( pWindow, bSystemWindow );
}

void Window::ImplResetReallyVisible()
{
    bool bBecameReallyInvisible = mpWindowImpl->mbReallyVisible;

    mbDevOutput     = false;
    mpWindowImpl->mbReallyVisible = false;
    mpWindowImpl->mbReallyShown   = false;

    // the SHOW/HIDE events serve as indicators to send child creation/destroy events to the access bridge.
    // For this, the data member of the event must not be NULL.
    // Previously, we did this in Window::Show, but there some events got lost in certain situations.
    if( bBecameReallyInvisible && ImplIsAccessibleCandidate() )
        ImplCallEventListeners( VCLEVENT_WINDOW_HIDE, this );
        // TODO. It's kind of a hack that we're re-using the VCLEVENT_WINDOW_HIDE. Normally, we should
        // introduce another event which explicitly triggers the Accessibility implementations.

    Window* pWindow = mpWindowImpl->mpFirstOverlap;
    while ( pWindow )
    {
        if ( pWindow->mpWindowImpl->mbReallyVisible )
            pWindow->ImplResetReallyVisible();
        pWindow = pWindow->mpWindowImpl->mpNext;
    }

    pWindow = mpWindowImpl->mpFirstChild;
    while ( pWindow )
    {
        if ( pWindow->mpWindowImpl->mbReallyVisible )
            pWindow->ImplResetReallyVisible();
        pWindow = pWindow->mpWindowImpl->mpNext;
    }
}

void Window::ImplUpdateWindowPtr( Window* pWindow )
{
    if ( mpWindowImpl->mpFrameWindow != pWindow->mpWindowImpl->mpFrameWindow )
    {
        // release graphic
        OutputDevice *pOutDev = GetOutDev();
        pOutDev->ReleaseGraphics();
    }

    mpWindowImpl->mpFrameData     = pWindow->mpWindowImpl->mpFrameData;
    mpWindowImpl->mpFrame         = pWindow->mpWindowImpl->mpFrame;
    mpWindowImpl->mpFrameWindow   = pWindow->mpWindowImpl->mpFrameWindow;
    if ( pWindow->ImplIsOverlapWindow() )
        mpWindowImpl->mpOverlapWindow = pWindow;
    else
        mpWindowImpl->mpOverlapWindow = pWindow->mpWindowImpl->mpOverlapWindow;

    Window* pChild = mpWindowImpl->mpFirstChild;
    while ( pChild )
    {
        pChild->ImplUpdateWindowPtr( pWindow );
        pChild = pChild->mpWindowImpl->mpNext;
    }
}

void Window::ImplUpdateWindowPtr()
{
    Window* pChild = mpWindowImpl->mpFirstChild;
    while ( pChild )
    {
        pChild->ImplUpdateWindowPtr( this );
        pChild = pChild->mpWindowImpl->mpNext;
    }
}

void Window::ImplUpdateOverlapWindowPtr( bool bNewFrame )
{
    bool bVisible = IsVisible();
    Show( false );
    ImplRemoveWindow( bNewFrame );
    Window* pRealParent = mpWindowImpl->mpRealParent;
    ImplInsertWindow( ImplGetParent() );
    mpWindowImpl->mpRealParent = pRealParent;
    ImplUpdateWindowPtr();
    if ( ImplUpdatePos() )
        ImplUpdateSysObjPos();

    if ( bNewFrame )
    {
        Window* pOverlapWindow = mpWindowImpl->mpFirstOverlap;
        while ( pOverlapWindow )
        {
            Window* pNextOverlapWindow = pOverlapWindow->mpWindowImpl->mpNext;
            pOverlapWindow->ImplUpdateOverlapWindowPtr( bNewFrame );
            pOverlapWindow = pNextOverlapWindow;
        }
    }

    if ( bVisible )
        Show( true );
}

SystemWindow* Window::GetSystemWindow() const
{

    const Window* pWin = this;
    while ( pWin && !pWin->IsSystemWindow() )
        pWin  = pWin->GetParent();
    return (SystemWindow*)pWin;
}

static SystemWindow *ImplGetLastSystemWindow( Window *pWin )
{
    // get the most top-level system window, the one that contains the taskpanelist
    SystemWindow *pSysWin = NULL;
    if( !pWin )
        return pSysWin;
    Window *pMyParent = pWin;
    while ( pMyParent )
    {
        if ( pMyParent->IsSystemWindow() )
            pSysWin = (SystemWindow*)pMyParent;
        pMyParent = pMyParent->GetParent();
    }
    return pSysWin;
}

void Window::SetParent( Window* pNewParent )
{
    DBG_ASSERT( pNewParent, "Window::SetParent(): pParent == NULL" );
    DBG_ASSERT( pNewParent != this, "someone tried to reparent a window to itself" );

    if( pNewParent == this )
        return;

    // check if the taskpanelist would change and move the window pointer accordingly
    SystemWindow *pSysWin = ImplGetLastSystemWindow(this);
    SystemWindow *pNewSysWin = NULL;
    bool bChangeTaskPaneList = false;
    if( pSysWin && pSysWin->ImplIsInTaskPaneList( this ) )
    {
        pNewSysWin = ImplGetLastSystemWindow( pNewParent );
        if( pNewSysWin && pNewSysWin != pSysWin )
        {
            bChangeTaskPaneList = true;
            pSysWin->GetTaskPaneList()->RemoveWindow( this );
        }
    }
    // remove ownerdraw decorated windows from list in the top-most frame window
    if( (GetStyle() & WB_OWNERDRAWDECORATION) && mpWindowImpl->mbFrame )
    {
        ::std::vector< Window* >& rList = ImplGetOwnerDrawList();
        ::std::vector< Window* >::iterator p;
        p = ::std::find( rList.begin(), rList.end(), this );
        if( p != rList.end() )
            rList.erase( p );
    }

    ImplSetFrameParent( pNewParent );

    if ( mpWindowImpl->mpBorderWindow )
    {
        mpWindowImpl->mpRealParent = pNewParent;
        mpWindowImpl->mpBorderWindow->SetParent( pNewParent );
        return;
    }

    if ( mpWindowImpl->mpParent == pNewParent )
        return;

    if ( mpWindowImpl->mbFrame )
        mpWindowImpl->mpFrame->SetParent( pNewParent->mpWindowImpl->mpFrame );

    bool bVisible = IsVisible();
    Show( false, SHOW_NOFOCUSCHANGE );

    // check if the overlap window changes
    Window* pOldOverlapWindow;
    Window* pNewOverlapWindow = NULL;
    if ( ImplIsOverlapWindow() )
        pOldOverlapWindow = NULL;
    else
    {
        pNewOverlapWindow = pNewParent->ImplGetFirstOverlapWindow();
        if ( mpWindowImpl->mpOverlapWindow != pNewOverlapWindow )
            pOldOverlapWindow = mpWindowImpl->mpOverlapWindow;
        else
            pOldOverlapWindow = NULL;
    }

    // convert windows in the hierarchy
    bool bFocusOverlapWin = HasChildPathFocus( true );
    bool bFocusWin = HasChildPathFocus();
    bool bNewFrame = pNewParent->mpWindowImpl->mpFrameWindow != mpWindowImpl->mpFrameWindow;
    if ( bNewFrame )
    {
        if ( mpWindowImpl->mpFrameData->mpFocusWin )
        {
            if ( IsWindowOrChild( mpWindowImpl->mpFrameData->mpFocusWin ) )
                mpWindowImpl->mpFrameData->mpFocusWin = NULL;
        }
        if ( mpWindowImpl->mpFrameData->mpMouseMoveWin )
        {
            if ( IsWindowOrChild( mpWindowImpl->mpFrameData->mpMouseMoveWin ) )
                mpWindowImpl->mpFrameData->mpMouseMoveWin = NULL;
        }
        if ( mpWindowImpl->mpFrameData->mpMouseDownWin )
        {
            if ( IsWindowOrChild( mpWindowImpl->mpFrameData->mpMouseDownWin ) )
                mpWindowImpl->mpFrameData->mpMouseDownWin = NULL;
        }
    }
    ImplRemoveWindow( bNewFrame );
    ImplInsertWindow( pNewParent );
    if ( mpWindowImpl->mnParentClipMode & PARENTCLIPMODE_CLIP )
        pNewParent->mpWindowImpl->mbClipChildren = true;
    ImplUpdateWindowPtr();
    if ( ImplUpdatePos() )
        ImplUpdateSysObjPos();

    // If the Overlap-Window has changed, we need to test whether
    // OverlapWindows that had the Child window as their parent
    // need to be put into the window hierarchy.
    if ( ImplIsOverlapWindow() )
    {
        if ( bNewFrame )
        {
            Window* pOverlapWindow = mpWindowImpl->mpFirstOverlap;
            while ( pOverlapWindow )
            {
                Window* pNextOverlapWindow = pOverlapWindow->mpWindowImpl->mpNext;
                pOverlapWindow->ImplUpdateOverlapWindowPtr( bNewFrame );
                pOverlapWindow = pNextOverlapWindow;
            }
        }
    }
    else if ( pOldOverlapWindow )
    {
        // reset Focus-Save
        if ( bFocusWin ||
             (pOldOverlapWindow->mpWindowImpl->mpLastFocusWindow &&
              IsWindowOrChild( pOldOverlapWindow->mpWindowImpl->mpLastFocusWindow )) )
            pOldOverlapWindow->mpWindowImpl->mpLastFocusWindow = NULL;

        Window* pOverlapWindow = pOldOverlapWindow->mpWindowImpl->mpFirstOverlap;
        while ( pOverlapWindow )
        {
            Window* pNextOverlapWindow = pOverlapWindow->mpWindowImpl->mpNext;
            if ( ImplIsRealParentPath( pOverlapWindow->ImplGetWindow() ) )
                pOverlapWindow->ImplUpdateOverlapWindowPtr( bNewFrame );
            pOverlapWindow = pNextOverlapWindow;
        }

        // update activate-status at next overlap window
        if ( HasChildPathFocus( true ) )
            ImplCallFocusChangeActivate( pNewOverlapWindow, pOldOverlapWindow );
    }

    // also convert Activate-Status
    if ( bNewFrame )
    {
        if ( (GetType() == WINDOW_BORDERWINDOW) &&
             (ImplGetWindow()->GetType() == WINDOW_FLOATINGWINDOW) )
            ((ImplBorderWindow*)this)->SetDisplayActive( mpWindowImpl->mpFrameData->mbHasFocus );
    }

    // when required give focus to new frame if
    // FocusWindow is changed with SetParent()
    if ( bFocusOverlapWin )
    {
        mpWindowImpl->mpFrameData->mpFocusWin = Application::GetFocusWindow();
        if ( !mpWindowImpl->mpFrameData->mbHasFocus )
        {
            mpWindowImpl->mpFrame->ToTop( 0 );
        }
    }

    // Assure DragSource and DropTarget members are created
    if ( bNewFrame )
    {
            GetDropTarget();
    }

    if( bChangeTaskPaneList )
        pNewSysWin->GetTaskPaneList()->AddWindow( this );

    if( (GetStyle() & WB_OWNERDRAWDECORATION) && mpWindowImpl->mbFrame )
        ImplGetOwnerDrawList().push_back( this );

    if ( bVisible )
        Show( true, SHOW_NOFOCUSCHANGE | SHOW_NOACTIVATE );
}

sal_uInt16 Window::GetChildCount() const
{

    sal_uInt16  nChildCount = 0;
    Window* pChild = mpWindowImpl->mpFirstChild;
    while ( pChild )
    {
        nChildCount++;
        pChild = pChild->mpWindowImpl->mpNext;
    }

    return nChildCount;
}

Window* Window::GetChild( sal_uInt16 nChild ) const
{

    sal_uInt16  nChildCount = 0;
    Window* pChild = mpWindowImpl->mpFirstChild;
    while ( pChild )
    {
        if ( nChild == nChildCount )
            return pChild;
        pChild = pChild->mpWindowImpl->mpNext;
        nChildCount++;
    }

    return NULL;
}

Window* Window::GetWindow( sal_uInt16 nType ) const
{

    switch ( nType )
    {
        case WINDOW_PARENT:
            return mpWindowImpl->mpRealParent;

        case WINDOW_FIRSTCHILD:
            return mpWindowImpl->mpFirstChild;

        case WINDOW_LASTCHILD:
            return mpWindowImpl->mpLastChild;

        case WINDOW_PREV:
            return mpWindowImpl->mpPrev;

        case WINDOW_NEXT:
            return mpWindowImpl->mpNext;

        case WINDOW_FIRSTOVERLAP:
            return mpWindowImpl->mpFirstOverlap;

        case WINDOW_LASTOVERLAP:
            return mpWindowImpl->mpLastOverlap;

        case WINDOW_OVERLAP:
            if ( ImplIsOverlapWindow() )
                return (Window*)this;
            else
                return mpWindowImpl->mpOverlapWindow;

        case WINDOW_PARENTOVERLAP:
            if ( ImplIsOverlapWindow() )
                return mpWindowImpl->mpOverlapWindow;
            else
                return mpWindowImpl->mpOverlapWindow->mpWindowImpl->mpOverlapWindow;

        case WINDOW_CLIENT:
            return ((Window*)this)->ImplGetWindow();

        case WINDOW_REALPARENT:
            return ImplGetParent();

        case WINDOW_FRAME:
            return mpWindowImpl->mpFrameWindow;

        case WINDOW_BORDER:
            if ( mpWindowImpl->mpBorderWindow )
                return mpWindowImpl->mpBorderWindow->GetWindow( WINDOW_BORDER );
            return (Window*)this;

        case WINDOW_FIRSTTOPWINDOWCHILD:
            return ImplGetWinData()->maTopWindowChildren.empty() ? NULL : *ImplGetWinData()->maTopWindowChildren.begin();

        case WINDOW_LASTTOPWINDOWCHILD:
            return ImplGetWinData()->maTopWindowChildren.empty() ? NULL : *ImplGetWinData()->maTopWindowChildren.rbegin();

        case WINDOW_PREVTOPWINDOWSIBLING:
        {
            if ( !mpWindowImpl->mpRealParent )
                return NULL;
            const ::std::list< Window* >& rTopWindows( mpWindowImpl->mpRealParent->ImplGetWinData()->maTopWindowChildren );
            ::std::list< Window* >::const_iterator myPos =
                ::std::find( rTopWindows.begin(), rTopWindows.end(), this );
            if ( myPos == rTopWindows.end() )
                return NULL;
            if ( myPos == rTopWindows.begin() )
                return NULL;
            return *--myPos;
        }

        case WINDOW_NEXTTOPWINDOWSIBLING:
        {
            if ( !mpWindowImpl->mpRealParent )
                return NULL;
            const ::std::list< Window* >& rTopWindows( mpWindowImpl->mpRealParent->ImplGetWinData()->maTopWindowChildren );
            ::std::list< Window* >::const_iterator myPos =
                ::std::find( rTopWindows.begin(), rTopWindows.end(), this );
            if ( ( myPos == rTopWindows.end() ) || ( ++myPos == rTopWindows.end() ) )
                return NULL;
            return *myPos;
        }

    }

    return NULL;
}

bool Window::IsChild( const Window* pWindow, bool bSystemWindow ) const
{
    do
    {
        if ( !bSystemWindow && pWindow->ImplIsOverlapWindow() )
            break;

        pWindow = pWindow->ImplGetParent();

        if ( pWindow == this )
            return true;
    }
    while ( pWindow );

    return false;
}

bool Window::IsWindowOrChild( const Window* pWindow, bool bSystemWindow ) const
{

    if ( this == pWindow )
        return true;
    return ImplIsChild( pWindow, bSystemWindow );
}

void Window::ImplSetFrameParent( const Window* pParent )
{
    Window* pFrameWindow = ImplGetSVData()->maWinData.mpFirstFrame;
    while( pFrameWindow )
    {
        // search all frames that are children of this window
        // and reparent them
        if( ImplIsRealParentPath( pFrameWindow ) )
        {
            DBG_ASSERT( mpWindowImpl->mpFrame != pFrameWindow->mpWindowImpl->mpFrame, "SetFrameParent to own" );
            DBG_ASSERT( mpWindowImpl->mpFrame, "no frame" );
            SalFrame* pParentFrame = pParent ? pParent->mpWindowImpl->mpFrame : NULL;
            pFrameWindow->mpWindowImpl->mpFrame->SetParent( pParentFrame );
        }
        pFrameWindow = pFrameWindow->mpWindowImpl->mpFrameData->mpNextFrame;
    }
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
