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

#include "fupage.hxx"

#include <sfx2/viewfrm.hxx>

// arrange Tab-Page

#include <svx/svxids.hrc>
#include <svx/dialogs.hrc>
#include <svl/itempool.hxx>
#include <vcl/msgbox.hxx>
#include <sfx2/request.hxx>
#include <svl/stritem.hxx>
#include <vcl/prntypes.hxx>
#include <svl/style.hxx>
#include <stlsheet.hxx>
#include <svx/svdorect.hxx>
#include <svx/svdundo.hxx>
#include <editeng/eeitem.hxx>
#include <editeng/frmdiritem.hxx>
#include <svx/xbtmpit.hxx>
#include <svx/xsetit.hxx>
#include <editeng/ulspitem.hxx>
#include <editeng/lrspitem.hxx>
#include <svx/sdr/properties/properties.hxx>

#include "glob.hrc"
#include <editeng/shaditem.hxx>
#include <editeng/boxitem.hxx>
#include <editeng/sizeitem.hxx>
#include <editeng/pbinitem.hxx>
#include <sfx2/app.hxx>
#include <sfx2/opengrf.hxx>

#include "strings.hrc"
#include "sdpage.hxx"
#include "View.hxx"
#include "Window.hxx"
#include "pres.hxx"
#include "drawdoc.hxx"
#include "DrawDocShell.hxx"
#include "ViewShell.hxx"
#include "DrawViewShell.hxx"
#include "app.hrc"
#include "unchss.hxx"
#include "undoback.hxx"
#include "sdabstdlg.hxx"
#include "sdresid.hxx"
#include "sdundogr.hxx"
#include "helpids.h"

#include <boost/scoped_ptr.hpp>

using namespace com::sun::star;

namespace sd {

class Window;

// 50 cm 28350
// adapted from writer
#define MAXHEIGHT 28350
#define MAXWIDTH  28350

TYPEINIT1( FuPage, FuPoor );

void mergeItemSetsImpl( SfxItemSet& rTarget, const SfxItemSet& rSource )
{
    const sal_uInt16* pPtr = rSource.GetRanges();
    sal_uInt16 p1, p2;
    while( *pPtr )
    {
        p1 = pPtr[0];
        p2 = pPtr[1];

        // make ranges discrete
        while(pPtr[2] && (pPtr[2] - p2 == 1))
        {
            p2 = pPtr[3];
            pPtr += 2;
        }
        rTarget.MergeRange( p1, p2 );
        pPtr += 2;
    }

    rTarget.Put(rSource);
}

FuPage::FuPage( ViewShell* pViewSh, ::sd::Window* pWin, ::sd::View* pView,
                 SdDrawDocument* pDoc, SfxRequest& rReq )
:   FuPoor(pViewSh, pWin, pView, pDoc, rReq),
    mrReq(rReq),
    mpArgs( rReq.GetArgs() ),
    mpBackgroundObjUndoAction( 0 ),
    mbPageBckgrdDeleted( false ),
    mbMasterPage( false ),
    mbDisplayBackgroundTabPage( true ),
    mpPage(0),
    mpDrawViewShell(0)
{
}

rtl::Reference<FuPoor> FuPage::Create( ViewShell* pViewSh, ::sd::Window* pWin, ::sd::View* pView, SdDrawDocument* pDoc, SfxRequest& rReq )
{
    rtl::Reference<FuPoor> xFunc( new FuPage( pViewSh, pWin, pView, pDoc, rReq ) );
    xFunc->DoExecute(rReq);
    return xFunc;
}

void FuPage::DoExecute( SfxRequest& )
{
    mpDrawViewShell = dynamic_cast<DrawViewShell*>(mpViewShell);
    DBG_ASSERT( mpDrawViewShell, "sd::FuPage::FuPage(), called without a current DrawViewShell!" );
    if( mpDrawViewShell )
    {
        mbMasterPage = mpDrawViewShell->GetEditMode() == EM_MASTERPAGE;
        mbDisplayBackgroundTabPage = (mpDrawViewShell->GetPageKind() == PK_STANDARD);
        mpPage = mpDrawViewShell->getCurrentPage();
    }

    if( mpPage )
    {
        // if there are no arguments given, open the dialog
        if( !mpArgs )
        {
            mpView->SdrEndTextEdit();
            mpArgs = ExecuteDialog(mpWindow);
        }

        // if we now have arguments, apply them to current page
        if( mpArgs )
            ApplyItemSet( mpArgs );
    }
}

FuPage::~FuPage()
{
    delete mpBackgroundObjUndoAction;
}

void FuPage::Activate()
{
}

void FuPage::Deactivate()
{
}

const SfxItemSet* FuPage::ExecuteDialog( Window* pParent )
{
    if (!mpDrawViewShell)
        return NULL;

    PageKind ePageKind = mpDrawViewShell->GetPageKind();

    SfxItemSet aNewAttr(mpDoc->GetPool(),
                        mpDoc->GetPool().GetWhich(SID_ATTR_LRSPACE),
                        mpDoc->GetPool().GetWhich(SID_ATTR_ULSPACE),
                        SID_ATTR_PAGE, SID_ATTR_PAGE_BSP,
                        SID_ATTR_BORDER_OUTER, SID_ATTR_BORDER_OUTER,
                        SID_ATTR_BORDER_SHADOW, SID_ATTR_BORDER_SHADOW,
                        XATTR_FILL_FIRST, XATTR_FILL_LAST,
                        EE_PARA_WRITINGDIR, EE_PARA_WRITINGDIR,
                        0);

    // Retrieve additional data for dialog

    SvxShadowItem aShadowItem(SID_ATTR_BORDER_SHADOW);
    aNewAttr.Put( aShadowItem );
    SvxBoxItem aBoxItem( SID_ATTR_BORDER_OUTER );
    aNewAttr.Put( aBoxItem );

    aNewAttr.Put( SvxFrameDirectionItem(
        mpDoc->GetDefaultWritingMode() == ::com::sun::star::text::WritingMode_RL_TB ? FRMDIR_HORI_RIGHT_TOP : FRMDIR_HORI_LEFT_TOP,
        EE_PARA_WRITINGDIR ) );

    // Retrieve page-data for dialog

    SvxPageItem aPageItem( SID_ATTR_PAGE );
    aPageItem.SetDescName( mpPage->GetName() );
    aPageItem.SetPageUsage( (SvxPageUsage) SVX_PAGE_ALL );
    aPageItem.SetLandscape( mpPage->GetOrientation() == ORIENTATION_LANDSCAPE ? sal_True: sal_False );
    aPageItem.SetNumType( mpDoc->GetPageNumType() );
    aNewAttr.Put( aPageItem );

    // size
    maSize = mpPage->GetSize();
    SvxSizeItem aSizeItem( SID_ATTR_PAGE_SIZE, maSize );
    aNewAttr.Put( aSizeItem );

    // Max size
    SvxSizeItem aMaxSizeItem( SID_ATTR_PAGE_MAXSIZE, Size( MAXWIDTH, MAXHEIGHT ) );
    aNewAttr.Put( aMaxSizeItem );

    // paperbin
    SvxPaperBinItem aPaperBinItem( SID_ATTR_PAGE_PAPERBIN, (const sal_uInt8)mpPage->GetPaperBin() );
    aNewAttr.Put( aPaperBinItem );

    SvxLRSpaceItem aLRSpaceItem( (sal_uInt16)mpPage->GetLftBorder(), (sal_uInt16)mpPage->GetRgtBorder(), 0, 0, mpDoc->GetPool().GetWhich(SID_ATTR_LRSPACE));
    aNewAttr.Put( aLRSpaceItem );

    SvxULSpaceItem aULSpaceItem( (sal_uInt16)mpPage->GetUppBorder(), (sal_uInt16)mpPage->GetLwrBorder(), mpDoc->GetPool().GetWhich(SID_ATTR_ULSPACE));
    aNewAttr.Put( aULSpaceItem );

    // Applikation
    bool bScale = mpDoc->GetDocumentType() != DOCUMENT_TYPE_DRAW;
    aNewAttr.Put( SfxBoolItem( SID_ATTR_PAGE_EXT1, bScale ? sal_True : sal_False ) );

    bool bFullSize = mpPage->IsMasterPage() ?
        mpPage->IsBackgroundFullSize() : ((SdPage&)mpPage->TRG_GetMasterPage()).IsBackgroundFullSize();

    aNewAttr.Put( SfxBoolItem( SID_ATTR_PAGE_EXT2, bFullSize ) );

    // Merge ItemSet for dialog

    const sal_uInt16* pPtr = aNewAttr.GetRanges();
    sal_uInt16 p1 = pPtr[0], p2 = pPtr[1];
    while(pPtr[2] && (pPtr[2] - p2 == 1))
    {
        p2 = pPtr[3];
        pPtr += 2;
    }
    SfxItemSet aMergedAttr( *aNewAttr.GetPool(), p1, p2 );

    mergeItemSetsImpl( aMergedAttr, aNewAttr );

    SdStyleSheet* pStyleSheet = mpPage->getPresentationStyle(HID_PSEUDOSHEET_BACKGROUND);

    // merge page background filling to the dialogs input set
    if( mbDisplayBackgroundTabPage )
    {
        if( mbMasterPage )
        {
            if(pStyleSheet)
                mergeItemSetsImpl( aMergedAttr, pStyleSheet->GetItemSet() );
        }
        else
        {
            // Only this page, get attributes for background fill
            const SfxItemSet& rBackgroundAttributes = mpPage->getSdrPageProperties().GetItemSet();

            if(drawing::FillStyle_NONE != ((const XFillStyleItem&)rBackgroundAttributes.Get(XATTR_FILLSTYLE)).GetValue())
            {
                // page attributes are used, take them
                aMergedAttr.Put(rBackgroundAttributes);
            }
            else
            {
                if(pStyleSheet
                    && drawing::FillStyle_NONE != ((const XFillStyleItem&)pStyleSheet->GetItemSet().Get(XATTR_FILLSTYLE)).GetValue())
                {
                    // if the page has no fill style, use the settings from the
                    // background stylesheet (if used)
                    mergeItemSetsImpl(aMergedAttr, pStyleSheet->GetItemSet());
                }
                else
                {
                    // no fill style from page, start with no fill style
                    aMergedAttr.Put(XFillStyleItem(drawing::FillStyle_NONE));
                }
            }
        }
    }

    boost::scoped_ptr< SfxItemSet > pTempSet;

    if( GetSlotID() == SID_SELECT_BACKGROUND )
    {
        SvxOpenGraphicDialog    aDlg(SdResId(STR_SET_BACKGROUND_PICTURE));

        if( aDlg.Execute() == GRFILTER_OK )
        {
            Graphic     aGraphic;
            int nError = aDlg.GetGraphic(aGraphic);
            if( nError == GRFILTER_OK )
            {
                pTempSet.reset( new SfxItemSet( mpDoc->GetPool(), XATTR_FILL_FIRST, XATTR_FILL_LAST, 0) );

                pTempSet->Put( XFillStyleItem( drawing::FillStyle_BITMAP ) );

                // MigrateItemSet makes sure the XFillBitmapItem will have a unique name
                SfxItemSet aMigrateSet( mpDoc->GetPool(), XATTR_FILLBITMAP, XATTR_FILLBITMAP );
                aMigrateSet.Put(XFillBitmapItem(OUString("background"), aGraphic));
                SdrModel::MigrateItemSet( &aMigrateSet, pTempSet.get(), mpDoc );

                pTempSet->Put( XFillBmpStretchItem( true ));
                pTempSet->Put( XFillBmpTileItem( false ));
            }
        }
    }
    else
    {
        // create the dialog
        SdAbstractDialogFactory* pFact = SdAbstractDialogFactory::Create();
        boost::scoped_ptr<SfxAbstractTabDialog> pDlg( pFact ? pFact->CreateSdTabPageDialog(NULL, &aMergedAttr, mpDocSh, mbDisplayBackgroundTabPage ) : 0 );
        if( pDlg.get() && pDlg->Execute() == RET_OK )
            pTempSet.reset( new SfxItemSet(*pDlg->GetOutputItemSet()) );
    }

    if (pTempSet.get() && pStyleSheet)
    {
        pStyleSheet->AdjustToFontHeight(*pTempSet);

        if( mbDisplayBackgroundTabPage )
        {
            // if some fillstyle-items are not set in the dialog, then
            // try to use the items before
            bool bChanges = false;
            for( sal_uInt16 i=XATTR_FILL_FIRST; i<XATTR_FILL_LAST; i++ )
            {
                if( aMergedAttr.GetItemState( i ) != SFX_ITEM_DEFAULT )
                {
                    if( pTempSet->GetItemState( i ) == SFX_ITEM_DEFAULT )
                        pTempSet->Put( aMergedAttr.Get( i ) );
                    else
                        if( aMergedAttr.GetItem( i ) != pTempSet->GetItem( i ) )
                            bChanges = true;
                }
            }

            // if the background for this page was set to invisible, the background-object has to be deleted, too.
            if( ( ( (XFillStyleItem*) pTempSet->GetItem( XATTR_FILLSTYLE ) )->GetValue() == drawing::FillStyle_NONE ) ||
                ( ( pTempSet->GetItemState( XATTR_FILLSTYLE ) == SFX_ITEM_DEFAULT ) &&
                    ( ( (XFillStyleItem*) aMergedAttr.GetItem( XATTR_FILLSTYLE ) )->GetValue() == drawing::FillStyle_NONE ) ) )
                mbPageBckgrdDeleted = true;

            bool bSetToAllPages = false;

            // Ask, whether the setting are for the background-page or for the current page
            if( !mbMasterPage && bChanges )
            {
                // But don't ask in notice-view, because we can't change the background of
                // notice-masterpage (at the moment)
                if( ePageKind != PK_NOTES )
                {
                    MessBox aQuestionBox (
                        pParent,
                        WB_YES_NO | WB_DEF_YES,
                        SD_RESSTR(STR_PAGE_BACKGROUND_TITLE),
                        SD_RESSTR(STR_PAGE_BACKGROUND_TXT) );
                    aQuestionBox.SetImage( QueryBox::GetStandardImage() );
                    bSetToAllPages = ( RET_YES == aQuestionBox.Execute() );
                }

                if( mbPageBckgrdDeleted )
                {
                    mpBackgroundObjUndoAction = new SdBackgroundObjUndoAction(
                        *mpDoc, *mpPage, mpPage->getSdrPageProperties().GetItemSet());

                    if(!mpPage->IsMasterPage())
                    {
                        // on normal pages, switch off fill attribute usage
                        SdrPageProperties& rPageProperties = mpPage->getSdrPageProperties();
                        rPageProperties.ClearItem( XATTR_FILLBITMAP );
                        rPageProperties.ClearItem( XATTR_FILLGRADIENT );
                        rPageProperties.ClearItem( XATTR_FILLHATCH );
                        rPageProperties.PutItem(XFillStyleItem(drawing::FillStyle_NONE));
                    }
                }
            }
            /* Special treatment: reset the INVALIDS to
               NULL-Pointer (otherwise INVALIDs or pointer point
               to DefaultItems in the template; both would
               prevent the attribute inheritance) */
            pTempSet->ClearInvalidItems();

            if( mbMasterPage )
            {
                StyleSheetUndoAction* pAction = new StyleSheetUndoAction(mpDoc, (SfxStyleSheet*)pStyleSheet, &(*pTempSet.get()));
                mpDocSh->GetUndoManager()->AddUndoAction(pAction);
                pStyleSheet->GetItemSet().Put( *(pTempSet.get()) );
                sdr::properties::CleanupFillProperties( pStyleSheet->GetItemSet() );
                pStyleSheet->Broadcast(SfxSimpleHint(SFX_HINT_DATACHANGED));
            }
            else if( bSetToAllPages )
            {
                OUString aComment(SdResId(STR_UNDO_CHANGE_PAGEFORMAT));
                ::svl::IUndoManager* pUndoMgr = mpDocSh->GetUndoManager();
                pUndoMgr->EnterListAction(aComment, aComment);
                SdUndoGroup* pUndoGroup = new SdUndoGroup(mpDoc);
                pUndoGroup->SetComment(aComment);

                //Set background on all master pages
                sal_uInt16 nMasterPageCount = mpDoc->GetMasterSdPageCount(ePageKind);
                for (sal_uInt16 i = 0; i < nMasterPageCount; ++i)
                {
                    SdPage *pMasterPage = mpDoc->GetMasterSdPage(i, ePageKind);
                    SdStyleSheet *pStyle =
                        pMasterPage->getPresentationStyle(HID_PSEUDOSHEET_BACKGROUND);
                    StyleSheetUndoAction* pAction =
                        new StyleSheetUndoAction(mpDoc, (SfxStyleSheet*)pStyle, &(*pTempSet.get()));
                    pUndoGroup->AddAction(pAction);
                    pStyle->GetItemSet().Put( *(pTempSet.get()) );
                    sdr::properties::CleanupFillProperties( pStyleSheet->GetItemSet() );
                    pStyle->Broadcast(SfxSimpleHint(SFX_HINT_DATACHANGED));
                }

                //Remove background from all pages to reset to the master bg
                sal_uInt16 nPageCount(mpDoc->GetSdPageCount(ePageKind));
                for(sal_uInt16 i=0; i<nPageCount; ++i)
                {
                    SdPage *pPage = mpDoc->GetSdPage(i, ePageKind);

                    const SfxItemSet& rFillAttributes = pPage->getSdrPageProperties().GetItemSet();
                       if(drawing::FillStyle_NONE != ((const XFillStyleItem&)rFillAttributes.Get(XATTR_FILLSTYLE)).GetValue())
                    {
                        SdBackgroundObjUndoAction *pBackgroundObjUndoAction = new SdBackgroundObjUndoAction(*mpDoc, *pPage, rFillAttributes);
                        pUndoGroup->AddAction(pBackgroundObjUndoAction);

                        SdrPageProperties& rPageProperties = pPage->getSdrPageProperties();
                        rPageProperties.ClearItem( XATTR_FILLBITMAP );
                        rPageProperties.ClearItem( XATTR_FILLGRADIENT );
                        rPageProperties.ClearItem( XATTR_FILLHATCH );
                        rPageProperties.PutItem(XFillStyleItem(drawing::FillStyle_NONE));

                        pPage->ActionChanged();
                    }
                }

                pUndoMgr->AddUndoAction(pUndoGroup);
                pUndoMgr->LeaveListAction();

            }

            // if background filling is set to master pages then clear from page set
            if( mbMasterPage || bSetToAllPages )
            {
                for( sal_uInt16 nWhich = XATTR_FILL_FIRST; nWhich <= XATTR_FILL_LAST; nWhich++ )
                {
                    pTempSet->ClearItem( nWhich );
                }
                pTempSet->Put(XFillStyleItem(drawing::FillStyle_NONE));
            }

            const SfxPoolItem *pItem;
            if( SFX_ITEM_SET == pTempSet->GetItemState( EE_PARA_WRITINGDIR, false, &pItem ) )
            {
                sal_uInt32 nVal = ((SvxFrameDirectionItem*)pItem)->GetValue();
                mpDoc->SetDefaultWritingMode( nVal == FRMDIR_HORI_RIGHT_TOP ? ::com::sun::star::text::WritingMode_RL_TB : ::com::sun::star::text::WritingMode_LR_TB );
            }

            mpDoc->SetChanged(true);

            // BackgroundFill of Masterpage: no hard attributes allowed
            SdrPage& rUsedMasterPage = mpPage->IsMasterPage() ? *mpPage : mpPage->TRG_GetMasterPage();
            OSL_ENSURE(rUsedMasterPage.IsMasterPage(), "No MasterPage (!)");
            rUsedMasterPage.getSdrPageProperties().ClearItem();
            OSL_ENSURE(0 != rUsedMasterPage.getSdrPageProperties().GetStyleSheet(),
                "MasterPage without StyleSheet detected (!)");
        }

        aNewAttr.Put(*(pTempSet.get()));
        mrReq.Done( aNewAttr );

        return mrReq.GetArgs();
    }
    else
    {
        return 0;
    }
}

void FuPage::ApplyItemSet( const SfxItemSet* pArgs )
{
    if (!pArgs || !mpDrawViewShell)
        return;

    // Set new page-attributes
    PageKind ePageKind = mpDrawViewShell->GetPageKind();
    const SfxPoolItem*  pPoolItem;
    bool                bSetPageSizeAndBorder = false;
    Size                aNewSize(maSize);
    sal_Int32               nLeft  = -1, nRight = -1, nUpper = -1, nLower = -1;
    bool                bScaleAll = true;
    Orientation         eOrientation = mpPage->GetOrientation();
    SdPage*             pMasterPage = mpPage->IsMasterPage() ? mpPage : &(SdPage&)(mpPage->TRG_GetMasterPage());
    bool                bFullSize = pMasterPage->IsBackgroundFullSize();
    sal_uInt16              nPaperBin = mpPage->GetPaperBin();

    if( pArgs->GetItemState(SID_ATTR_PAGE, true, &pPoolItem) == SFX_ITEM_SET )
    {
        mpDoc->SetPageNumType(((const SvxPageItem*) pPoolItem)->GetNumType());

        eOrientation = ((const SvxPageItem*) pPoolItem)->IsLandscape() ?
            ORIENTATION_LANDSCAPE : ORIENTATION_PORTRAIT;

        if( mpPage->GetOrientation() != eOrientation )
            bSetPageSizeAndBorder = true;

        mpDrawViewShell->ResetActualPage();
    }

    if( pArgs->GetItemState(SID_ATTR_PAGE_SIZE, true, &pPoolItem) == SFX_ITEM_SET )
    {
        aNewSize = ((const SvxSizeItem*) pPoolItem)->GetSize();

        if( mpPage->GetSize() != aNewSize )
            bSetPageSizeAndBorder = true;
    }

    if( pArgs->GetItemState(mpDoc->GetPool().GetWhich(SID_ATTR_LRSPACE),
                            true, &pPoolItem) == SFX_ITEM_SET )
    {
        nLeft = ((const SvxLRSpaceItem*) pPoolItem)->GetLeft();
        nRight = ((const SvxLRSpaceItem*) pPoolItem)->GetRight();

        if( mpPage->GetLftBorder() != nLeft || mpPage->GetRgtBorder() != nRight )
            bSetPageSizeAndBorder = true;

    }

    if( pArgs->GetItemState(mpDoc->GetPool().GetWhich(SID_ATTR_ULSPACE),
                            true, &pPoolItem) == SFX_ITEM_SET )
    {
        nUpper = ((const SvxULSpaceItem*) pPoolItem)->GetUpper();
        nLower = ((const SvxULSpaceItem*) pPoolItem)->GetLower();

        if( mpPage->GetUppBorder() != nUpper || mpPage->GetLwrBorder() != nLower )
            bSetPageSizeAndBorder = true;
    }

    if( pArgs->GetItemState(mpDoc->GetPool().GetWhich(SID_ATTR_PAGE_EXT1), true, &pPoolItem) == SFX_ITEM_SET )
    {
        bScaleAll = ((const SfxBoolItem*) pPoolItem)->GetValue();
    }

    if( pArgs->GetItemState(mpDoc->GetPool().GetWhich(SID_ATTR_PAGE_EXT2), true, &pPoolItem) == SFX_ITEM_SET )
    {
        bFullSize = ((const SfxBoolItem*) pPoolItem)->GetValue();

        if(pMasterPage->IsBackgroundFullSize() != bFullSize )
            bSetPageSizeAndBorder = true;
    }

    // Paper Bin
    if( pArgs->GetItemState(mpDoc->GetPool().GetWhich(SID_ATTR_PAGE_PAPERBIN), true, &pPoolItem) == SFX_ITEM_SET )
    {
        nPaperBin = ((const SvxPaperBinItem*) pPoolItem)->GetValue();

        if( mpPage->GetPaperBin() != nPaperBin )
            bSetPageSizeAndBorder = true;
    }

    if (nLeft == -1 && nUpper != -1)
    {
        bSetPageSizeAndBorder = true;
        nLeft  = mpPage->GetLftBorder();
        nRight = mpPage->GetRgtBorder();
    }
    else if (nLeft != -1 && nUpper == -1)
    {
        bSetPageSizeAndBorder = true;
        nUpper = mpPage->GetUppBorder();
        nLower = mpPage->GetLwrBorder();
    }

    if( bSetPageSizeAndBorder || !mbMasterPage )
        mpDrawViewShell->SetPageSizeAndBorder(ePageKind, aNewSize, nLeft, nRight, nUpper, nLower, bScaleAll, eOrientation, nPaperBin, bFullSize );

    // if bMasterPage==sal_False then create a background-object for this page with the
    // properties set in the dialog before, but if mbPageBckgrdDeleted==sal_True then
    // the background of this page was set to invisible, so it would be a mistake
    // to create a new background-object for this page !

    if( mbDisplayBackgroundTabPage )
    {
        if( !mbMasterPage && !mbPageBckgrdDeleted )
        {
            // Only this page
            delete mpBackgroundObjUndoAction;
            mpBackgroundObjUndoAction = new SdBackgroundObjUndoAction(
                *mpDoc, *mpPage, mpPage->getSdrPageProperties().GetItemSet());
            SfxItemSet aSet( *pArgs );
            sdr::properties::CleanupFillProperties(aSet);
            mpPage->getSdrPageProperties().ClearItem();
            mpPage->getSdrPageProperties().PutItemSet(aSet);
        }
    }

    // add undo action for background object
    if( mpBackgroundObjUndoAction )
    {
        // set merge flag, because a SdUndoGroupAction could have been inserted before
        mpDocSh->GetUndoManager()->AddUndoAction( mpBackgroundObjUndoAction, true );
        mpBackgroundObjUndoAction = 0;
    }

    // Objects can not be bigger than ViewSize
    Size aPageSize = mpDoc->GetSdPage(0, ePageKind)->GetSize();
    Size aViewSize = Size(aPageSize.Width() * 3, aPageSize.Height() * 2);
    mpDoc->SetMaxObjSize(aViewSize);

    // if necessary, we tell Preview the new context
    mpDrawViewShell->UpdatePreview( mpDrawViewShell->GetActualPage() );
}

} // end of namespace sd

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
