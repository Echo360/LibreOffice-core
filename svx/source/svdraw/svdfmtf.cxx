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

#include "svdfmtf.hxx"
#include <editeng/editdata.hxx>
#include <math.h>
#include <svx/xpoly.hxx>
#include <vcl/svapp.hxx>
#include <editeng/eeitem.hxx>
#include <editeng/fhgtitem.hxx>
#include <editeng/wghtitem.hxx>
#include <editeng/postitem.hxx>
#include <editeng/udlnitem.hxx>
#include <editeng/crossedoutitem.hxx>
#include <editeng/shdditem.hxx>
#include <svx/xlnclit.hxx>
#include <svx/xlncapit.hxx>
#include <svx/xlnwtit.hxx>
#include <svx/xflclit.hxx>
#include <svx/xgrad.hxx>
#include <svx/xflgrit.hxx>
#include <editeng/fontitem.hxx>
#include <editeng/autokernitem.hxx>
#include <editeng/wrlmitem.hxx>
#include <editeng/contouritem.hxx>
#include <editeng/colritem.hxx>
#include <vcl/metric.hxx>
#include <editeng/charscaleitem.hxx>
#include <svx/xflhtit.hxx>
#include <svx/svdattr.hxx>
#include <svx/svdmodel.hxx>
#include <svx/svdpage.hxx>
#include <svx/svdobj.hxx>
#include <svx/svdotext.hxx>
#include <svx/svdorect.hxx>
#include <svx/svdocirc.hxx>
#include <svx/svdograf.hxx>
#include <svx/svdopath.hxx>
#include <svx/svdetc.hxx>
#include <svl/itemset.hxx>
#include <basegfx/polygon/b2dpolygon.hxx>
#include <tools/helpers.hxx>
#include <basegfx/matrix/b2dhommatrix.hxx>
#include <basegfx/matrix/b2dhommatrixtools.hxx>
#include <svx/xlinjoit.hxx>
#include <svx/xlndsit.hxx>
#include <basegfx/polygon/b2dpolygonclipper.hxx>
#include <svx/xbtmpit.hxx>
#include <svx/xfltrit.hxx>
#include <vcl/bmpacc.hxx>
#include <svx/xflbmtit.hxx>
#include <svx/xflbstit.hxx>
#include <svx/svdpntv.hxx>
#include <basegfx/polygon/b2dpolypolygontools.hxx>
#include <svx/svditer.hxx>
#include <svx/svdogrp.hxx>

using namespace com::sun::star;

ImpSdrGDIMetaFileImport::ImpSdrGDIMetaFileImport(
    SdrModel& rModel,
    SdrLayerID nLay,
    const Rectangle& rRect)
:   maTmpList(),
    maVD(),
    maScaleRect(rRect),
    mnMapScalingOfs(0),
    mpLineAttr(0),
    mpFillAttr(0),
    mpTextAttr(0),
    mpModel(&rModel),
    mnLayer(nLay),
    maOldLineColor(),
    mnLineWidth(0),
    maLineJoin(basegfx::B2DLINEJOIN_NONE),
    maLineCap(com::sun::star::drawing::LineCap_BUTT),
    maDash(XDASH_RECT, 0, 0, 0, 0, 0),
    mbMov(false),
    mbSize(false),
    maOfs(0, 0),
    mfScaleX(1.0),
    mfScaleY(1.0),
    maScaleX(1.0),
    maScaleY(1.0),
    mbFntDirty(true),
    mbLastObjWasPolyWithoutLine(false),
    mbNoLine(false),
    mbNoFill(false),
    mbLastObjWasLine(false),
    maClip()
{
    maVD.EnableOutput(false);
    maVD.SetLineColor();
    maVD.SetFillColor();
    maOldLineColor.SetRed( maVD.GetLineColor().GetRed() + 1 );
    mpLineAttr = new SfxItemSet(rModel.GetItemPool(), XATTR_LINE_FIRST, XATTR_LINE_LAST, 0, 0);
    mpFillAttr = new SfxItemSet(rModel.GetItemPool(), XATTR_FILL_FIRST, XATTR_FILL_LAST, 0, 0);
    mpTextAttr = new SfxItemSet(rModel.GetItemPool(), EE_ITEMS_START, EE_ITEMS_END, 0, 0);
    checkClip();
}

ImpSdrGDIMetaFileImport::~ImpSdrGDIMetaFileImport()
{
    delete mpLineAttr;
    delete mpFillAttr;
    delete mpTextAttr;
}

void ImpSdrGDIMetaFileImport::DoLoopActions(GDIMetaFile& rMtf, SvdProgressInfo* pProgrInfo, sal_uInt32* pActionsToReport)
{
    const sal_uLong nCount(rMtf.GetActionSize());

    for(sal_uLong a(0); a < nCount; a++)
    {
        MetaAction* pAct = rMtf.GetAction(a);

        if(!pAct)
        {
            OSL_ENSURE(false, "OOps, no action at valid position (!)");
            pAct = rMtf.GetAction(0);
        }

        switch (pAct->GetType())
        {
            case META_PIXEL_ACTION          : DoAction((MetaPixelAction          &)*pAct); break;
            case META_POINT_ACTION          : DoAction((MetaPointAction          &)*pAct); break;
            case META_LINE_ACTION           : DoAction((MetaLineAction           &)*pAct); break;
            case META_RECT_ACTION           : DoAction((MetaRectAction           &)*pAct); break;
            case META_ROUNDRECT_ACTION      : DoAction((MetaRoundRectAction      &)*pAct); break;
            case META_ELLIPSE_ACTION        : DoAction((MetaEllipseAction        &)*pAct); break;
            case META_ARC_ACTION            : DoAction((MetaArcAction            &)*pAct); break;
            case META_PIE_ACTION            : DoAction((MetaPieAction            &)*pAct); break;
            case META_CHORD_ACTION          : DoAction((MetaChordAction          &)*pAct); break;
            case META_POLYLINE_ACTION       : DoAction((MetaPolyLineAction       &)*pAct); break;
            case META_POLYGON_ACTION        : DoAction((MetaPolygonAction        &)*pAct); break;
            case META_POLYPOLYGON_ACTION    : DoAction((MetaPolyPolygonAction    &)*pAct); break;
            case META_TEXT_ACTION           : DoAction((MetaTextAction           &)*pAct); break;
            case META_TEXTARRAY_ACTION      : DoAction((MetaTextArrayAction      &)*pAct); break;
            case META_STRETCHTEXT_ACTION    : DoAction((MetaStretchTextAction    &)*pAct); break;
            case META_BMP_ACTION            : DoAction((MetaBmpAction            &)*pAct); break;
            case META_BMPSCALE_ACTION       : DoAction((MetaBmpScaleAction       &)*pAct); break;
            case META_BMPEX_ACTION          : DoAction((MetaBmpExAction          &)*pAct); break;
            case META_BMPEXSCALE_ACTION     : DoAction((MetaBmpExScaleAction     &)*pAct); break;
            case META_LINECOLOR_ACTION      : DoAction((MetaLineColorAction      &)*pAct); break;
            case META_FILLCOLOR_ACTION      : DoAction((MetaFillColorAction      &)*pAct); break;
            case META_TEXTCOLOR_ACTION      : DoAction((MetaTextColorAction      &)*pAct); break;
            case META_TEXTFILLCOLOR_ACTION  : DoAction((MetaTextFillColorAction  &)*pAct); break;
            case META_FONT_ACTION           : DoAction((MetaFontAction           &)*pAct); break;
            case META_TEXTALIGN_ACTION      : DoAction((MetaTextAlignAction      &)*pAct); break;
            case META_MAPMODE_ACTION        : DoAction((MetaMapModeAction        &)*pAct); break;
            case META_CLIPREGION_ACTION     : DoAction((MetaClipRegionAction     &)*pAct); break;
            case META_MOVECLIPREGION_ACTION : DoAction((MetaMoveClipRegionAction &)*pAct); break;
            case META_ISECTRECTCLIPREGION_ACTION: DoAction((MetaISectRectClipRegionAction&)*pAct); break;
            case META_ISECTREGIONCLIPREGION_ACTION: DoAction((MetaISectRegionClipRegionAction&)*pAct); break;
            case META_RASTEROP_ACTION       : DoAction((MetaRasterOpAction       &)*pAct); break;
            case META_PUSH_ACTION           : DoAction((MetaPushAction           &)*pAct); break;
            case META_POP_ACTION            : DoAction((MetaPopAction            &)*pAct); break;
            case META_HATCH_ACTION          : DoAction((MetaHatchAction          &)*pAct); break;

            // #i125211# MetaCommentAction may change index, thus hand it over
            case META_COMMENT_ACTION        : DoAction((MetaCommentAction&)*pAct, rMtf, a);
                break;

            // missing actions added
            case META_TEXTRECT_ACTION       : DoAction((MetaTextRectAction&)*pAct); break;
            case META_BMPSCALEPART_ACTION   : DoAction((MetaBmpScalePartAction&)*pAct); break;
            case META_BMPEXSCALEPART_ACTION : DoAction((MetaBmpExScalePartAction&)*pAct); break;
            case META_MASK_ACTION           : DoAction((MetaMaskAction&)*pAct); break;
            case META_MASKSCALE_ACTION      : DoAction((MetaMaskScaleAction&)*pAct); break;
            case META_MASKSCALEPART_ACTION  : DoAction((MetaMaskScalePartAction&)*pAct); break;
            case META_GRADIENT_ACTION       : DoAction((MetaGradientAction&)*pAct); break;
            case META_WALLPAPER_ACTION      : DoAction((MetaWallpaperAction&)*pAct); break;
            case META_TRANSPARENT_ACTION    : DoAction((MetaTransparentAction&)*pAct); break;
            case META_EPS_ACTION            : DoAction((MetaEPSAction&)*pAct); break;
            case META_REFPOINT_ACTION       : DoAction((MetaRefPointAction&)*pAct); break;
            case META_TEXTLINECOLOR_ACTION  : DoAction((MetaTextLineColorAction&)*pAct); break;
            case META_TEXTLINE_ACTION       : DoAction((MetaTextLineAction&)*pAct); break;
            case META_FLOATTRANSPARENT_ACTION : DoAction((MetaFloatTransparentAction&)*pAct); break;
            case META_GRADIENTEX_ACTION     : DoAction((MetaGradientExAction&)*pAct); break;
            case META_LAYOUTMODE_ACTION     : DoAction((MetaLayoutModeAction&)*pAct); break;
            case META_TEXTLANGUAGE_ACTION   : DoAction((MetaTextLanguageAction&)*pAct); break;
            case META_OVERLINECOLOR_ACTION  : DoAction((MetaOverlineColorAction&)*pAct); break;
        }

        if(pProgrInfo && pActionsToReport)
        {
            (*pActionsToReport)++;

            if(*pActionsToReport >= 16) // update all 16 actions
            {
                if(!pProgrInfo->ReportActions(*pActionsToReport))
                    break;

                *pActionsToReport = 0;
            }
        }
    }
}

sal_uInt32 ImpSdrGDIMetaFileImport::DoImport(
    const GDIMetaFile& rMtf,
    SdrObjList& rOL,
    sal_uLong nInsPos,
    SvdProgressInfo* pProgrInfo)
{
    // setup some global scale parameter
    // mfScaleX, mfScaleY, maScaleX, maScaleY, mbMov, mbSize
    mfScaleX = mfScaleY = 1.0;
    const Size aMtfSize(rMtf.GetPrefSize());

    if(aMtfSize.Width() & aMtfSize.Height() && (!maScaleRect.IsEmpty()))
    {
        maOfs = maScaleRect.TopLeft();

        if(aMtfSize.Width() != (maScaleRect.GetWidth() - 1))
        {
            mfScaleX = (double)( maScaleRect.GetWidth() - 1 ) / (double)aMtfSize.Width();
        }

        if(aMtfSize.Height() != (maScaleRect.GetHeight() - 1))
        {
            mfScaleY = (double)( maScaleRect.GetHeight() - 1 ) / (double)aMtfSize.Height();
        }
    }

    mbMov = maOfs.X()!=0 || maOfs.Y()!=0;
    mbSize = false;
    maScaleX = Fraction( 1, 1 );
    maScaleY = Fraction( 1, 1 );

    if(aMtfSize.Width() != (maScaleRect.GetWidth() - 1))
    {
        maScaleX = Fraction(maScaleRect.GetWidth() - 1, aMtfSize.Width());
        mbSize = true;
    }

    if(aMtfSize.Height() != (maScaleRect.GetHeight() - 1))
    {
        maScaleY = Fraction(maScaleRect.GetHeight() - 1, aMtfSize.Height());
        mbSize = true;
    }

    if(pProgrInfo)
    {
        pProgrInfo->SetActionCount(rMtf.GetActionSize());
    }

    sal_uInt32 nActionsToReport(0);

    // execute
    DoLoopActions(const_cast< GDIMetaFile& >(rMtf), pProgrInfo, &nActionsToReport);

    if(pProgrInfo)
    {
        pProgrInfo->ReportActions(nActionsToReport);
        nActionsToReport = 0;
    }

    // MapMode scaling
    MapScaling();

    // To calculate the progress meter, we use GetActionSize()*3.
    // However, maTmpList has a lower entry count limit than GetActionSize(),
    // so the actions that were assumed were too much have to be re-added.
    nActionsToReport = (rMtf.GetActionSize() - maTmpList.size()) * 2;

    // announce all currently unannounced rescales
    if(pProgrInfo)
    {
        pProgrInfo->ReportRescales(nActionsToReport);
        pProgrInfo->SetInsertCount(maTmpList.size());
    }

    nActionsToReport = 0;

    // insert all objects cached in aTmpList now into rOL from nInsPos
    if(nInsPos > rOL.GetObjCount())
    {
        nInsPos = rOL.GetObjCount();
    }

    SdrInsertReason aReason(SDRREASON_VIEWCALL);

    for(sal_uInt32 i(0); i < maTmpList.size(); i++)
    {
        SdrObject* pObj = maTmpList[i];
        rOL.NbcInsertObject(pObj, nInsPos, &aReason);
        nInsPos++;

        if(pProgrInfo)
        {
            nActionsToReport++;

            if(nActionsToReport >= 32) // update all 32 actions
            {
                pProgrInfo->ReportInserts(nActionsToReport);
                nActionsToReport = 0;
            }
        }
    }

    // report all remaining inserts for the last time
    if(pProgrInfo)
    {
        pProgrInfo->ReportInserts(nActionsToReport);
    }

    return maTmpList.size();
}

void ImpSdrGDIMetaFileImport::SetAttributes(SdrObject* pObj, bool bForceTextAttr)
{
    mbNoLine = false;
    mbNoFill = false;
    bool bLine(!bForceTextAttr);
    bool bFill(!pObj || (pObj->IsClosedObj() && !bForceTextAttr));
    bool bText(bForceTextAttr || (pObj && pObj->GetOutlinerParaObject()));

    if(bLine)
    {
        if(mnLineWidth)
        {
            mpLineAttr->Put(XLineWidthItem(mnLineWidth));
        }
        else
        {
            mpLineAttr->Put(XLineWidthItem(0));
        }

        maOldLineColor = maVD.GetLineColor();

        if(maVD.IsLineColor())
        {
            mpLineAttr->Put(XLineStyleItem(XLINE_SOLID));
            mpLineAttr->Put(XLineColorItem(OUString(), maVD.GetLineColor()));
        }
        else
        {
            mpLineAttr->Put(XLineStyleItem(XLINE_NONE));
        }

        switch(maLineJoin)
        {
            default : // basegfx::B2DLINEJOIN_NONE
                mpLineAttr->Put(XLineJointItem(com::sun::star::drawing::LineJoint_NONE));
                break;
            case basegfx::B2DLINEJOIN_MIDDLE:
                mpLineAttr->Put(XLineJointItem(com::sun::star::drawing::LineJoint_MIDDLE));
                break;
            case basegfx::B2DLINEJOIN_BEVEL:
                mpLineAttr->Put(XLineJointItem(com::sun::star::drawing::LineJoint_BEVEL));
                break;
            case basegfx::B2DLINEJOIN_MITER:
                mpLineAttr->Put(XLineJointItem(com::sun::star::drawing::LineJoint_MITER));
                break;
            case basegfx::B2DLINEJOIN_ROUND:
                mpLineAttr->Put(XLineJointItem(com::sun::star::drawing::LineJoint_ROUND));
                break;
        }

        // Add LineCap support
        mpLineAttr->Put(XLineCapItem(maLineCap));

        if(((maDash.GetDots() && maDash.GetDotLen()) || (maDash.GetDashes() && maDash.GetDashLen())) && maDash.GetDistance())
        {
            mpLineAttr->Put(XLineDashItem(OUString(), maDash));
        }
        else
        {
            mpLineAttr->Put(XLineDashItem(OUString(), XDash(XDASH_RECT)));
        }
    }
    else
    {
        mbNoLine = true;
    }

    if(bFill)
    {
        if(maVD.IsFillColor())
        {
            mpFillAttr->Put(XFillStyleItem(drawing::FillStyle_SOLID));
            mpFillAttr->Put(XFillColorItem(OUString(), maVD.GetFillColor()));
        }
        else
        {
            mpFillAttr->Put(XFillStyleItem(drawing::FillStyle_NONE));
        }
    }
    else
    {
        mbNoFill = true;
    }

    if(bText && mbFntDirty)
    {
        Font aFnt(maVD.GetFont());
        const sal_uInt32 nHeight(FRound(aFnt.GetSize().Height() * mfScaleY));

        mpTextAttr->Put( SvxFontItem( aFnt.GetFamily(), aFnt.GetName(), aFnt.GetStyleName(), aFnt.GetPitch(), aFnt.GetCharSet(), EE_CHAR_FONTINFO ) );
        mpTextAttr->Put( SvxFontItem( aFnt.GetFamily(), aFnt.GetName(), aFnt.GetStyleName(), aFnt.GetPitch(), aFnt.GetCharSet(), EE_CHAR_FONTINFO_CJK ) );
        mpTextAttr->Put( SvxFontItem( aFnt.GetFamily(), aFnt.GetName(), aFnt.GetStyleName(), aFnt.GetPitch(), aFnt.GetCharSet(), EE_CHAR_FONTINFO_CTL ) );
        mpTextAttr->Put(SvxPostureItem(aFnt.GetItalic(), EE_CHAR_ITALIC));
        mpTextAttr->Put(SvxWeightItem(aFnt.GetWeight(), EE_CHAR_WEIGHT));
        mpTextAttr->Put( SvxFontHeightItem( nHeight, 100, EE_CHAR_FONTHEIGHT ) );
        mpTextAttr->Put( SvxFontHeightItem( nHeight, 100, EE_CHAR_FONTHEIGHT_CJK ) );
        mpTextAttr->Put( SvxFontHeightItem( nHeight, 100, EE_CHAR_FONTHEIGHT_CTL ) );
        mpTextAttr->Put(SvxCharScaleWidthItem(100, EE_CHAR_FONTWIDTH));
        mpTextAttr->Put(SvxUnderlineItem(aFnt.GetUnderline(), EE_CHAR_UNDERLINE));
        mpTextAttr->Put(SvxOverlineItem(aFnt.GetOverline(), EE_CHAR_OVERLINE));
        mpTextAttr->Put(SvxCrossedOutItem(aFnt.GetStrikeout(), EE_CHAR_STRIKEOUT));
        mpTextAttr->Put(SvxShadowedItem(aFnt.IsShadow(), EE_CHAR_SHADOW));

        // #i118485# Setting this item leads to problems (written #i118498# for this)
        // mpTextAttr->Put(SvxAutoKernItem(aFnt.IsKerning(), EE_CHAR_KERNING));

        mpTextAttr->Put(SvxWordLineModeItem(aFnt.IsWordLineMode(), EE_CHAR_WLM));
        mpTextAttr->Put(SvxContourItem(aFnt.IsOutline(), EE_CHAR_OUTLINE));
        mpTextAttr->Put(SvxColorItem(maVD.GetTextColor(), EE_CHAR_COLOR));
        //... svxfont textitem svditext
        mbFntDirty = false;
    }

    if(pObj)
    {
        pObj->SetLayer(mnLayer);

        if(bLine)
        {
            pObj->SetMergedItemSet(*mpLineAttr);
        }

        if(bFill)
        {
            pObj->SetMergedItemSet(*mpFillAttr);
        }

        if(bText)
        {
            pObj->SetMergedItemSet(*mpTextAttr);
            pObj->SetMergedItem(SdrTextHorzAdjustItem(SDRTEXTHORZADJUST_LEFT));
        }
    }
}

void ImpSdrGDIMetaFileImport::InsertObj(SdrObject* pObj, bool bScale)
{
    if(bScale && !maScaleRect.IsEmpty())
    {
        if(mbSize)
        {
            pObj->NbcResize(Point(), maScaleX, maScaleY);
        }

        if(mbMov)
        {
            pObj->NbcMove(Size(maOfs.X(), maOfs.Y()));
        }
    }

    if(isClip())
    {
        const basegfx::B2DPolyPolygon aPoly(pObj->TakeXorPoly());
        const basegfx::B2DRange aOldRange(aPoly.getB2DRange());
        const SdrLayerID aOldLayer(pObj->GetLayer());
        const SfxItemSet aOldItemSet(pObj->GetMergedItemSet());
        const SdrGrafObj* pSdrGrafObj = dynamic_cast< SdrGrafObj* >(pObj);
        const SdrTextObj* pSdrTextObj = dynamic_cast< SdrTextObj* >(pObj);

        if(pSdrTextObj && pSdrTextObj->HasText())
        {
            // all text objects are created from ImportText and have no line or fill attributes, so
            // it is okay to concentrate on the text itself
            while(true)
            {
                const basegfx::B2DPolyPolygon aTextContour(pSdrTextObj->TakeContour());
                const basegfx::B2DRange aTextRange(aTextContour.getB2DRange());
                const basegfx::B2DRange aClipRange(maClip.getB2DRange());

                // no overlap -> completely outside
                if(!aClipRange.overlaps(aTextRange))
                {
                    SdrObject::Free(pObj);
                    break;
                }

                // when the clip is a rectangle fast check for inside is possible
                if(basegfx::tools::isRectangle(maClip) && aClipRange.isInside(aTextRange))
                {
                    // completely inside ClipRect
                    break;
                }

                // here text needs to be clipped; to do so, convert to SdrObjects with polygons
                // and add these recursively. Delete original object, do not add in this run
                SdrObject* pConverted = pSdrTextObj->ConvertToPolyObj(true, true);
                SdrObject::Free(pObj);

                if(pConverted)
                {
                    // recursively add created conversion; per definition this shall not
                    // contain further SdrTextObjs. Visit only non-group objects
                    SdrObjListIter aIter(*pConverted, IM_DEEPNOGROUPS);

                    // work with clones; the created conversion may contain group objects
                    // and when working with the original objects the loop itself could
                    // break and the cleanup later would be pretty complicated (only delete group
                    // objects, are these empty, ...?)
                    while(aIter.IsMore())
                    {
                        SdrObject* pCandidate = aIter.Next();
                        OSL_ENSURE(pCandidate && 0 == dynamic_cast< SdrObjGroup* >(pCandidate), "SdrObjListIter with IM_DEEPNOGROUPS error (!)");
                        SdrObject* pNewClone = pCandidate->Clone();

                        if(pNewClone)
                        {
                            InsertObj(pNewClone, false);
                        }
                        else
                        {
                            OSL_ENSURE(false, "SdrObject::Clone() failed (!)");
                        }
                    }

                    // cleanup temporary conversion objects
                    SdrObject::Free(pConverted);
                }

                break;
            }
        }
        else
        {
            BitmapEx aBitmapEx;

            if(pSdrGrafObj)
            {
                aBitmapEx = pSdrGrafObj->GetGraphic().GetBitmapEx();
            }

            SdrObject::Free(pObj);

            if(!aOldRange.isEmpty())
            {
                // clip against ClipRegion
                const basegfx::B2DPolyPolygon aNewPoly(
                    basegfx::tools::clipPolyPolygonOnPolyPolygon(
                        aPoly,
                        maClip,
                        true,
                        aPoly.isClosed() ? false : true));
                const basegfx::B2DRange aNewRange(aNewPoly.getB2DRange());

                if(!aNewRange.isEmpty())
                {
                    pObj = new SdrPathObj(
                        aNewPoly.isClosed() ? OBJ_POLY : OBJ_PLIN,
                        aNewPoly);

                    pObj->SetLayer(aOldLayer);
                    pObj->SetMergedItemSet(aOldItemSet);

                    if(!!aBitmapEx)
                    {
                        // aNewRange is inside of aOldRange and defines which part of aBitmapEx is used
                        const double fScaleX(aBitmapEx.GetSizePixel().Width() / (aOldRange.getWidth() ? aOldRange.getWidth() : 1.0));
                        const double fScaleY(aBitmapEx.GetSizePixel().Height() / (aOldRange.getHeight() ? aOldRange.getHeight() : 1.0));
                        basegfx::B2DRange aPixel(aNewRange);
                        basegfx::B2DHomMatrix aTrans;

                        aTrans.translate(-aOldRange.getMinX(), -aOldRange.getMinY());
                        aTrans.scale(fScaleX, fScaleY);
                        aPixel.transform(aTrans);

                        const Size aOrigSizePixel(aBitmapEx.GetSizePixel());
                        const Point aClipTopLeft(
                            basegfx::fround(floor(std::max(0.0, aPixel.getMinX()))),
                            basegfx::fround(floor(std::max(0.0, aPixel.getMinY()))));
                        const Size aClipSize(
                            basegfx::fround(ceil(std::min((double)aOrigSizePixel.Width(), aPixel.getWidth()))),
                            basegfx::fround(ceil(std::min((double)aOrigSizePixel.Height(), aPixel.getHeight()))));
                        const BitmapEx aClippedBitmap(
                            aBitmapEx,
                            aClipTopLeft,
                            aClipSize);

                        pObj->SetMergedItem(XFillStyleItem(drawing::FillStyle_BITMAP));
                        pObj->SetMergedItem(XFillBitmapItem(OUString(), Graphic(aClippedBitmap)));
                        pObj->SetMergedItem(XFillBmpTileItem(false));
                        pObj->SetMergedItem(XFillBmpStretchItem(true));
                    }
                }
            }
        }
    }

    if(pObj)
    {
        // #i111954# check object for visibility
        // used are SdrPathObj, SdrRectObj, SdrCircObj, SdrGrafObj
        bool bVisible(false);

        if(pObj->HasLineStyle())
        {
            bVisible = true;
        }

        if(!bVisible && pObj->HasFillStyle())
        {
            bVisible = true;
        }

        if(!bVisible)
        {
            SdrTextObj* pTextObj = dynamic_cast< SdrTextObj* >(pObj);

            if(pTextObj && pTextObj->HasText())
            {
                bVisible = true;
            }
        }

        if(!bVisible)
        {
            SdrGrafObj* pGrafObj = dynamic_cast< SdrGrafObj* >(pObj);

            if(pGrafObj)
            {
                // this may be refined to check if the graphic really is visible. It
                // is here to ensure that graphic objects without fill, line and text
                // get created
                bVisible = true;
            }
        }

        if(!bVisible)
        {
            SdrObject::Free(pObj);
        }
        else
        {
            maTmpList.push_back(pObj);

            if(dynamic_cast< SdrPathObj* >(pObj))
            {
                const bool bClosed(pObj->IsClosedObj());

                mbLastObjWasPolyWithoutLine = mbNoLine && bClosed;
                mbLastObjWasLine = !bClosed;
            }
            else
            {
                mbLastObjWasPolyWithoutLine = false;
                mbLastObjWasLine = false;
            }
        }
    }
}

/**************************************************************************************************/
void ImpSdrGDIMetaFileImport::DoAction(MetaPixelAction& /*rAct*/) const
{
}

void ImpSdrGDIMetaFileImport::DoAction(MetaPointAction& /*rAct*/) const
{
}

void ImpSdrGDIMetaFileImport::DoAction(MetaLineAction& rAct)
{
    // #i73407# reformulation to use new B2DPolygon classes
    const basegfx::B2DPoint aStart(rAct.GetStartPoint().X(), rAct.GetStartPoint().Y());
    const basegfx::B2DPoint aEnd(rAct.GetEndPoint().X(), rAct.GetEndPoint().Y());

    if(!aStart.equal(aEnd))
    {
        basegfx::B2DPolygon aLine;
        const basegfx::B2DHomMatrix aTransform(basegfx::tools::createScaleTranslateB2DHomMatrix(mfScaleX, mfScaleY, maOfs.X(), maOfs.Y()));

        aLine.append(aStart);
        aLine.append(aEnd);
        aLine.transform(aTransform);

        const LineInfo& rLineInfo = rAct.GetLineInfo();
        const sal_Int32 nNewLineWidth(rLineInfo.GetWidth());
        bool bCreateLineObject(true);

        if(mbLastObjWasLine && (nNewLineWidth == mnLineWidth) && CheckLastLineMerge(aLine))
        {
            bCreateLineObject = false;
        }

        if(bCreateLineObject)
        {
            SdrPathObj* pPath = new SdrPathObj(OBJ_LINE, basegfx::B2DPolyPolygon(aLine));
            mnLineWidth = nNewLineWidth;
            maLineJoin = rLineInfo.GetLineJoin();
            maLineCap = rLineInfo.GetLineCap();
            maDash = XDash(XDASH_RECT,
                rLineInfo.GetDotCount(), rLineInfo.GetDotLen(),
                rLineInfo.GetDashCount(), rLineInfo.GetDashLen(),
                rLineInfo.GetDistance());
            SetAttributes(pPath);
            mnLineWidth = 0;
            maLineJoin = basegfx::B2DLINEJOIN_NONE;
            maDash = XDash();
            InsertObj(pPath, false);
        }
    }
}

void ImpSdrGDIMetaFileImport::DoAction(MetaRectAction& rAct)
{
    SdrRectObj* pRect=new SdrRectObj(rAct.GetRect());
    SetAttributes(pRect);
    InsertObj(pRect);
}

void ImpSdrGDIMetaFileImport::DoAction(MetaRoundRectAction& rAct)
{
    SdrRectObj* pRect=new SdrRectObj(rAct.GetRect());
    SetAttributes(pRect);
    long nRad=(rAct.GetHorzRound()+rAct.GetVertRound())/2;
    if (nRad!=0) {
        SfxItemSet aSet(*mpLineAttr->GetPool(), SDRATTR_ECKENRADIUS, SDRATTR_ECKENRADIUS, 0, 0);
        aSet.Put(SdrEckenradiusItem(nRad));
        pRect->SetMergedItemSet(aSet);
    }
    InsertObj(pRect);
}

/**************************************************************************************************/

void ImpSdrGDIMetaFileImport::DoAction(MetaEllipseAction& rAct)
{
    SdrCircObj* pCirc=new SdrCircObj(OBJ_CIRC,rAct.GetRect());
    SetAttributes(pCirc);
    InsertObj(pCirc);
}

void ImpSdrGDIMetaFileImport::DoAction(MetaArcAction& rAct)
{
    Point aCenter(rAct.GetRect().Center());
    long nStart=GetAngle(rAct.GetStartPoint()-aCenter);
    long nEnd=GetAngle(rAct.GetEndPoint()-aCenter);
    SdrCircObj* pCirc=new SdrCircObj(OBJ_CARC,rAct.GetRect(),nStart,nEnd);
    SetAttributes(pCirc);
    InsertObj(pCirc);
}

void ImpSdrGDIMetaFileImport::DoAction(MetaPieAction& rAct)
{
    Point aCenter(rAct.GetRect().Center());
    long nStart=GetAngle(rAct.GetStartPoint()-aCenter);
    long nEnd=GetAngle(rAct.GetEndPoint()-aCenter);
    SdrCircObj* pCirc=new SdrCircObj(OBJ_SECT,rAct.GetRect(),nStart,nEnd);
    SetAttributes(pCirc);
    InsertObj(pCirc);
}

void ImpSdrGDIMetaFileImport::DoAction(MetaChordAction& rAct)
{
    Point aCenter(rAct.GetRect().Center());
    long nStart=GetAngle(rAct.GetStartPoint()-aCenter);
    long nEnd=GetAngle(rAct.GetEndPoint()-aCenter);
    SdrCircObj* pCirc=new SdrCircObj(OBJ_CCUT,rAct.GetRect(),nStart,nEnd);
    SetAttributes(pCirc);
    InsertObj(pCirc);
}

/**************************************************************************************************/

bool ImpSdrGDIMetaFileImport::CheckLastLineMerge(const basegfx::B2DPolygon& rSrcPoly)
{
    // #i102706# Do not merge closed polygons
    if(rSrcPoly.isClosed())
    {
        return false;
    }

    // #i73407# reformulation to use new B2DPolygon classes
    if(mbLastObjWasLine && (maOldLineColor == maVD.GetLineColor()) && rSrcPoly.count())
    {
        SdrObject* pTmpObj = maTmpList.size() ? maTmpList[maTmpList.size() - 1] : 0;
        SdrPathObj* pLastPoly = dynamic_cast< SdrPathObj* >(pTmpObj);

        if(pLastPoly)
        {
            if(1L == pLastPoly->GetPathPoly().count())
            {
                bool bOk(false);
                basegfx::B2DPolygon aDstPoly(pLastPoly->GetPathPoly().getB2DPolygon(0L));

                // #i102706# Do not merge closed polygons
                if(aDstPoly.isClosed())
                {
                    return false;
                }

                if(aDstPoly.count())
                {
                    const sal_uInt32 nMaxDstPnt(aDstPoly.count() - 1L);
                    const sal_uInt32 nMaxSrcPnt(rSrcPoly.count() - 1L);

                    if(aDstPoly.getB2DPoint(nMaxDstPnt) == rSrcPoly.getB2DPoint(0L))
                    {
                        aDstPoly.append(rSrcPoly, 1L, rSrcPoly.count() - 1L);
                        bOk = true;
                    }
                    else if(aDstPoly.getB2DPoint(0L) == rSrcPoly.getB2DPoint(nMaxSrcPnt))
                    {
                        basegfx::B2DPolygon aNew(rSrcPoly);
                        aNew.append(aDstPoly, 1L, aDstPoly.count() - 1L);
                        aDstPoly = aNew;
                        bOk = true;
                    }
                    else if(aDstPoly.getB2DPoint(0L) == rSrcPoly.getB2DPoint(0L))
                    {
                        aDstPoly.flip();
                        aDstPoly.append(rSrcPoly, 1L, rSrcPoly.count() - 1L);
                        bOk = true;
                    }
                    else if(aDstPoly.getB2DPoint(nMaxDstPnt) == rSrcPoly.getB2DPoint(nMaxSrcPnt))
                    {
                        basegfx::B2DPolygon aNew(rSrcPoly);
                        aNew.flip();
                        aDstPoly.append(aNew, 1L, aNew.count() - 1L);
                        bOk = true;
                    }
                }

                if(bOk)
                {
                    pLastPoly->NbcSetPathPoly(basegfx::B2DPolyPolygon(aDstPoly));
                }

                return bOk;
            }
        }
    }

    return false;
}

bool ImpSdrGDIMetaFileImport::CheckLastPolyLineAndFillMerge(const basegfx::B2DPolyPolygon & rPolyPolygon)
{
    // #i73407# reformulation to use new B2DPolygon classes
    if(mbLastObjWasPolyWithoutLine)
    {
        SdrObject* pTmpObj = maTmpList.size() ? maTmpList[maTmpList.size() - 1] : 0;
        SdrPathObj* pLastPoly = dynamic_cast< SdrPathObj* >(pTmpObj);

        if(pLastPoly)
        {
            if(pLastPoly->GetPathPoly() == rPolyPolygon)
            {
                SetAttributes(NULL);

                if(!mbNoLine && mbNoFill)
                {
                    pLastPoly->SetMergedItemSet(*mpLineAttr);

                    return true;
                }
            }
        }
    }

    return false;
}

void ImpSdrGDIMetaFileImport::checkClip()
{
    if(maVD.IsClipRegion())
    {
        maClip = maVD.GetClipRegion().GetAsB2DPolyPolygon();

        if(isClip())
        {
            const basegfx::B2DHomMatrix aTransform(
                basegfx::tools::createScaleTranslateB2DHomMatrix(
                    mfScaleX,
                    mfScaleY,
                    maOfs.X(),
                    maOfs.Y()));

            maClip.transform(aTransform);
        }
    }
}

bool ImpSdrGDIMetaFileImport::isClip() const
{
    return !maClip.getB2DRange().isEmpty();
}

void ImpSdrGDIMetaFileImport::DoAction( MetaPolyLineAction& rAct )
{
    // #i73407# reformulation to use new B2DPolygon classes
    basegfx::B2DPolygon aSource(rAct.GetPolygon().getB2DPolygon());

    if(aSource.count())
    {
        const basegfx::B2DHomMatrix aTransform(basegfx::tools::createScaleTranslateB2DHomMatrix(mfScaleX, mfScaleY, maOfs.X(), maOfs.Y()));
        aSource.transform(aTransform);
    }

    const LineInfo& rLineInfo = rAct.GetLineInfo();
    const sal_Int32 nNewLineWidth(rLineInfo.GetWidth());
    bool bCreateLineObject(true);

    if(mbLastObjWasLine && (nNewLineWidth == mnLineWidth) && CheckLastLineMerge(aSource))
    {
        bCreateLineObject = false;
    }
    else if(mbLastObjWasPolyWithoutLine && CheckLastPolyLineAndFillMerge(basegfx::B2DPolyPolygon(aSource)))
    {
        bCreateLineObject = false;
    }

    if(bCreateLineObject)
    {
        SdrPathObj* pPath = new SdrPathObj(
            aSource.isClosed() ? OBJ_POLY : OBJ_PLIN,
            basegfx::B2DPolyPolygon(aSource));
        mnLineWidth = nNewLineWidth;
        maLineJoin = rLineInfo.GetLineJoin();
        maLineCap = rLineInfo.GetLineCap();
        maDash = XDash(XDASH_RECT,
            rLineInfo.GetDotCount(), rLineInfo.GetDotLen(),
            rLineInfo.GetDashCount(), rLineInfo.GetDashLen(),
            rLineInfo.GetDistance());
        SetAttributes(pPath);
        mnLineWidth = 0;
        maLineJoin = basegfx::B2DLINEJOIN_NONE;
        maDash = XDash();
        InsertObj(pPath, false);
    }
}

void ImpSdrGDIMetaFileImport::DoAction( MetaPolygonAction& rAct )
{
    // #i73407# reformulation to use new B2DPolygon classes
    basegfx::B2DPolygon aSource(rAct.GetPolygon().getB2DPolygon());

    if(aSource.count())
    {
        const basegfx::B2DHomMatrix aTransform(basegfx::tools::createScaleTranslateB2DHomMatrix(mfScaleX, mfScaleY, maOfs.X(), maOfs.Y()));
        aSource.transform(aTransform);

        if(!mbLastObjWasPolyWithoutLine || !CheckLastPolyLineAndFillMerge(basegfx::B2DPolyPolygon(aSource)))
        {
            // #i73407# make sure polygon is closed, it's a filled primitive
            aSource.setClosed(true);
            SdrPathObj* pPath = new SdrPathObj(OBJ_POLY, basegfx::B2DPolyPolygon(aSource));
            SetAttributes(pPath);
            InsertObj(pPath, false);
        }
    }
}

void ImpSdrGDIMetaFileImport::DoAction(MetaPolyPolygonAction& rAct)
{
    // #i73407# reformulation to use new B2DPolygon classes
    basegfx::B2DPolyPolygon aSource(rAct.GetPolyPolygon().getB2DPolyPolygon());

    if(aSource.count())
    {
        const basegfx::B2DHomMatrix aTransform(basegfx::tools::createScaleTranslateB2DHomMatrix(mfScaleX, mfScaleY, maOfs.X(), maOfs.Y()));
        aSource.transform(aTransform);

        if(!mbLastObjWasPolyWithoutLine || !CheckLastPolyLineAndFillMerge(aSource))
        {
            // #i73407# make sure polygon is closed, it's a filled primitive
            aSource.setClosed(true);
            SdrPathObj* pPath = new SdrPathObj(OBJ_POLY, aSource);
            SetAttributes(pPath);
            InsertObj(pPath, false);
        }
    }
}

/**************************************************************************************************/

void ImpSdrGDIMetaFileImport::ImportText( const Point& rPos, const OUString& rStr, const MetaAction& rAct )
{
    // calc text box size, add 5% to make it fit safely

    FontMetric aFontMetric( maVD.GetFontMetric() );
    Font aFnt( maVD.GetFont() );
    FontAlign eAlg( aFnt.GetAlign() );

    sal_Int32 nTextWidth = (sal_Int32)( maVD.GetTextWidth( rStr ) * mfScaleX );
    sal_Int32 nTextHeight = (sal_Int32)( maVD.GetTextHeight() * mfScaleY );

    Point aPos( FRound(rPos.X() * mfScaleX + maOfs.X()), FRound(rPos.Y() * mfScaleY + maOfs.Y()) );
    Size aSize( nTextWidth, nTextHeight );

    if ( eAlg == ALIGN_BASELINE )
        aPos.Y() -= FRound(aFontMetric.GetAscent() * mfScaleY);
    else if ( eAlg == ALIGN_BOTTOM )
        aPos.Y() -= nTextHeight;

    Rectangle aTextRect( aPos, aSize );
    SdrRectObj* pText =new SdrRectObj( OBJ_TEXT, aTextRect );

    pText->SetMergedItem ( makeSdrTextUpperDistItem (0));
    pText->SetMergedItem ( makeSdrTextLowerDistItem (0));
    pText->SetMergedItem ( makeSdrTextRightDistItem (0));
    pText->SetMergedItem ( makeSdrTextLeftDistItem (0));

    if ( aFnt.GetWidth() || ( rAct.GetType() == META_STRETCHTEXT_ACTION ) )
    {
        pText->ClearMergedItem( SDRATTR_TEXT_AUTOGROWWIDTH );
        pText->SetMergedItem( makeSdrTextAutoGrowHeightItem( false ) );
        // don't let the margins eat the space needed for the text
        pText->SetMergedItem( SdrTextFitToSizeTypeItem( SDRTEXTFIT_ALLLINES ) );
    }
    else
    {
        pText->SetMergedItem( makeSdrTextAutoGrowWidthItem( true ) );
    }

    pText->SetModel(mpModel);
    pText->SetLayer(mnLayer);
    pText->NbcSetText( rStr );
    SetAttributes( pText, true );
    pText->SetSnapRect( aTextRect );

    if (!aFnt.IsTransparent())
    {
        SfxItemSet aAttr(*mpFillAttr->GetPool(), XATTR_FILL_FIRST, XATTR_FILL_LAST, 0, 0);
        aAttr.Put(XFillStyleItem(drawing::FillStyle_SOLID));
        aAttr.Put(XFillColorItem(OUString(), aFnt.GetFillColor()));
        pText->SetMergedItemSet(aAttr);
    }
    sal_uInt32 nWink = aFnt.GetOrientation();
    if ( nWink )
    {
        nWink*=10;
        double a=nWink*nPi180;
        double nSin=sin(a);
        double nCos=cos(a);
        pText->NbcRotate(aPos,nWink,nSin,nCos);
    }
    InsertObj( pText, false );
}

void ImpSdrGDIMetaFileImport::DoAction(MetaTextAction& rAct)
{
    OUString aStr(rAct.GetText());
    aStr = aStr.copy(rAct.GetIndex(), rAct.GetLen());
    ImportText( rAct.GetPoint(), aStr, rAct );
}

void ImpSdrGDIMetaFileImport::DoAction(MetaTextArrayAction& rAct)
{
    OUString aStr(rAct.GetText());
    aStr = aStr.copy(rAct.GetIndex(), rAct.GetLen());
    ImportText( rAct.GetPoint(), aStr, rAct );
}

void ImpSdrGDIMetaFileImport::DoAction(MetaStretchTextAction& rAct)
{
    OUString aStr(rAct.GetText());
    aStr = aStr.copy(rAct.GetIndex(), rAct.GetLen());
    ImportText( rAct.GetPoint(), aStr, rAct );
}

void ImpSdrGDIMetaFileImport::DoAction(MetaBmpAction& rAct)
{
    Rectangle aRect(rAct.GetPoint(),rAct.GetBitmap().GetSizePixel());
    aRect.Right()++; aRect.Bottom()++;
    SdrGrafObj* pGraf=new SdrGrafObj(Graphic(rAct.GetBitmap()),aRect);

    // This action is not creating line and fill, set directly, do not use SetAttributes(..)
    pGraf->SetMergedItem(XLineStyleItem(XLINE_NONE));
    pGraf->SetMergedItem(XFillStyleItem(drawing::FillStyle_NONE));
    InsertObj(pGraf);
}

void ImpSdrGDIMetaFileImport::DoAction(MetaBmpScaleAction& rAct)
{
    Rectangle aRect(rAct.GetPoint(),rAct.GetSize());
    aRect.Right()++; aRect.Bottom()++;
    SdrGrafObj* pGraf=new SdrGrafObj(Graphic(rAct.GetBitmap()),aRect);

    // This action is not creating line and fill, set directly, do not use SetAttributes(..)
    pGraf->SetMergedItem(XLineStyleItem(XLINE_NONE));
    pGraf->SetMergedItem(XFillStyleItem(drawing::FillStyle_NONE));
    InsertObj(pGraf);
}

void ImpSdrGDIMetaFileImport::DoAction(MetaBmpExAction& rAct)
{
    Rectangle aRect(rAct.GetPoint(),rAct.GetBitmapEx().GetSizePixel());
    aRect.Right()++; aRect.Bottom()++;
    SdrGrafObj* pGraf=new SdrGrafObj( rAct.GetBitmapEx(), aRect );

    // This action is not creating line and fill, set directly, do not use SetAttributes(..)
    pGraf->SetMergedItem(XLineStyleItem(XLINE_NONE));
    pGraf->SetMergedItem(XFillStyleItem(drawing::FillStyle_NONE));
    InsertObj(pGraf);
}

void ImpSdrGDIMetaFileImport::DoAction(MetaBmpExScaleAction& rAct)
{
    Rectangle aRect(rAct.GetPoint(),rAct.GetSize());
    aRect.Right()++; aRect.Bottom()++;
    SdrGrafObj* pGraf=new SdrGrafObj( rAct.GetBitmapEx(), aRect );

    // This action is not creating line and fill, set directly, do not use SetAttributes(..)
    pGraf->SetMergedItem(XLineStyleItem(XLINE_NONE));
    pGraf->SetMergedItem(XFillStyleItem(drawing::FillStyle_NONE));
    InsertObj(pGraf);
}



void ImpSdrGDIMetaFileImport::DoAction( MetaHatchAction& rAct )
{
    // #i73407# reformulation to use new B2DPolygon classes
    basegfx::B2DPolyPolygon aSource(rAct.GetPolyPolygon().getB2DPolyPolygon());

    if(aSource.count())
    {
        const basegfx::B2DHomMatrix aTransform(basegfx::tools::createScaleTranslateB2DHomMatrix(mfScaleX, mfScaleY, maOfs.X(), maOfs.Y()));
        aSource.transform(aTransform);

        if(!mbLastObjWasPolyWithoutLine || !CheckLastPolyLineAndFillMerge(aSource))
        {
            const Hatch& rHatch = rAct.GetHatch();
            SdrPathObj* pPath = new SdrPathObj(OBJ_POLY, aSource);
            // #i125211# Use the ranges from the SdrObject to create a new empty SfxItemSet
            SfxItemSet aHatchAttr(mpModel->GetItemPool(), pPath->GetMergedItemSet().GetRanges());
            XHatchStyle eStyle;

            switch(rHatch.GetStyle())
            {
                case(HATCH_TRIPLE) :
                {
                    eStyle = XHATCH_TRIPLE;
                    break;
                }

                case(HATCH_DOUBLE) :
                {
                    eStyle = XHATCH_DOUBLE;
                    break;
                }

                default:
                {
                    eStyle = XHATCH_SINGLE;
                    break;
                }
            }

            SetAttributes(pPath);
            aHatchAttr.Put(XFillStyleItem(drawing::FillStyle_HATCH));
            aHatchAttr.Put(XFillHatchItem(&mpModel->GetItemPool(), XHatch(rHatch.GetColor(), eStyle, rHatch.GetDistance(), rHatch.GetAngle())));
            pPath->SetMergedItemSet(aHatchAttr);

            InsertObj(pPath, false);
        }
    }
}



void ImpSdrGDIMetaFileImport::DoAction(MetaLineColorAction& rAct)
{
    rAct.Execute(&maVD);
}

void ImpSdrGDIMetaFileImport::DoAction(MetaMapModeAction& rAct)
{
    MapScaling();
    rAct.Execute(&maVD);
    mbLastObjWasPolyWithoutLine = false;
    mbLastObjWasLine = false;
}

void ImpSdrGDIMetaFileImport::MapScaling()
{
    const size_t nAnz(maTmpList.size());
    const MapMode& rMap = maVD.GetMapMode();
    Point aMapOrg( rMap.GetOrigin() );
    bool bMov2(aMapOrg.X() != 0 || aMapOrg.Y() != 0);

    if(bMov2)
    {
        for(size_t i = mnMapScalingOfs; i < nAnz; i++)
        {
            SdrObject* pObj = maTmpList[i];

            pObj->NbcMove(Size(aMapOrg.X(), aMapOrg.Y()));
        }
    }

    mnMapScalingOfs = nAnz;
}



void ImpSdrGDIMetaFileImport::DoAction( MetaCommentAction& rAct, GDIMetaFile& rMtf, sal_uLong& a) // GDIMetaFile* pMtf )
{
    bool aSkipComment = false;

    if (a < rMtf.GetActionSize() && rAct.GetComment().equalsIgnoreAsciiCase("XGRAD_SEQ_BEGIN"))
    {
        // #i125211# Check if next action is a MetaGradientExAction
        MetaGradientExAction* pAct = dynamic_cast< MetaGradientExAction* >(rMtf.GetAction(a + 1));

        if( pAct && pAct->GetType() == META_GRADIENTEX_ACTION )
        {
            // #i73407# reformulation to use new B2DPolygon classes
            basegfx::B2DPolyPolygon aSource(pAct->GetPolyPolygon().getB2DPolyPolygon());

            if(aSource.count())
            {
                if(!mbLastObjWasPolyWithoutLine || !CheckLastPolyLineAndFillMerge(aSource))
                {
                    const Gradient& rGrad = pAct->GetGradient();
                    SdrPathObj* pPath = new SdrPathObj(OBJ_POLY, aSource);
                    // #i125211# Use the ranges from the SdrObject to create a new empty SfxItemSet
                    SfxItemSet aGradAttr(mpModel->GetItemPool(), pPath->GetMergedItemSet().GetRanges());
                    XGradient aXGradient;

                    aXGradient.SetGradientStyle((XGradientStyle)rGrad.GetStyle());
                    aXGradient.SetStartColor(rGrad.GetStartColor());
                    aXGradient.SetEndColor(rGrad.GetEndColor());
                    aXGradient.SetAngle((sal_uInt16)rGrad.GetAngle());
                    aXGradient.SetBorder(rGrad.GetBorder());
                    aXGradient.SetXOffset(rGrad.GetOfsX());
                    aXGradient.SetYOffset(rGrad.GetOfsY());
                    aXGradient.SetStartIntens(rGrad.GetStartIntensity());
                    aXGradient.SetEndIntens(rGrad.GetEndIntensity());
                    aXGradient.SetSteps(rGrad.GetSteps());

                    // no need to use SetAttributes(..) here since line and fill style
                    // need to be set individually
                    // SetAttributes(pPath);

                    // switch line off; if there was one there will be a
                    // META_POLYLINE_ACTION following creating another object
                    aGradAttr.Put(XLineStyleItem(XLINE_NONE));

                    // add detected gradient fillstyle
                    aGradAttr.Put(XFillStyleItem(drawing::FillStyle_GRADIENT));
                    aGradAttr.Put(XFillGradientItem(aXGradient));

                    pPath->SetMergedItemSet(aGradAttr);

                    InsertObj(pPath);
                }
            }

            aSkipComment = true;
        }
    }

    if(aSkipComment)
    {
        // #i125211# forward until closing MetaCommentAction
        MetaAction* pSkipAct = rMtf.GetAction(++a);

        while( pSkipAct
            && ((pSkipAct->GetType() != META_COMMENT_ACTION )
                || !(((MetaCommentAction*)pSkipAct)->GetComment().equalsIgnoreAsciiCase("XGRAD_SEQ_END"))))
        {
            pSkipAct = rMtf.GetAction(++a);
        }
    }
}

void ImpSdrGDIMetaFileImport::DoAction(MetaTextRectAction& rAct)
{
    GDIMetaFile aTemp;

    maVD.AddTextRectActions(rAct.GetRect(), rAct.GetText(), rAct.GetStyle(), aTemp);
    DoLoopActions(aTemp, 0, 0);
}

void ImpSdrGDIMetaFileImport::DoAction(MetaBmpScalePartAction& rAct)
{
    Rectangle aRect(rAct.GetDestPoint(), rAct.GetDestSize());
    Bitmap aBitmap(rAct.GetBitmap());

    aRect.Right()++;
    aRect.Bottom()++;
    aBitmap.Crop(Rectangle(rAct.GetSrcPoint(), rAct.GetSrcSize()));
    SdrGrafObj* pGraf = new SdrGrafObj(aBitmap, aRect);

    // This action is not creating line and fill, set directly, do not use SetAttributes(..)
    pGraf->SetMergedItem(XLineStyleItem(XLINE_NONE));
    pGraf->SetMergedItem(XFillStyleItem(drawing::FillStyle_NONE));
    InsertObj(pGraf);
}

void ImpSdrGDIMetaFileImport::DoAction(MetaBmpExScalePartAction& rAct)
{
    Rectangle aRect(rAct.GetDestPoint(),rAct.GetDestSize());
    BitmapEx aBitmapEx(rAct.GetBitmapEx());

    aRect.Right()++;
    aRect.Bottom()++;
    aBitmapEx.Crop(Rectangle(rAct.GetSrcPoint(), rAct.GetSrcSize()));
    SdrGrafObj* pGraf = new SdrGrafObj(aBitmapEx, aRect);

    // This action is not creating line and fill, set directly, do not use SetAttributes(..)
    pGraf->SetMergedItem(XLineStyleItem(XLINE_NONE));
    pGraf->SetMergedItem(XFillStyleItem(drawing::FillStyle_NONE));
    InsertObj(pGraf);
}

void ImpSdrGDIMetaFileImport::DoAction(MetaMaskAction& rAct)
{
    Rectangle aRect(rAct.GetPoint(), rAct.GetBitmap().GetSizePixel());
    BitmapEx aBitmapEx(rAct.GetBitmap(), rAct.GetColor());

    aRect.Right()++; aRect.Bottom()++;
    SdrGrafObj* pGraf = new SdrGrafObj(aBitmapEx, aRect);

    // This action is not creating line and fill, set directly, do not use SetAttributes(..)
    pGraf->SetMergedItem(XLineStyleItem(XLINE_NONE));
    pGraf->SetMergedItem(XFillStyleItem(drawing::FillStyle_NONE));
    InsertObj(pGraf);
}

void ImpSdrGDIMetaFileImport::DoAction(MetaMaskScaleAction& rAct)
{
    Rectangle aRect(rAct.GetPoint(), rAct.GetSize());
    BitmapEx aBitmapEx(rAct.GetBitmap(), rAct.GetColor());

    aRect.Right()++; aRect.Bottom()++;
    SdrGrafObj* pGraf = new SdrGrafObj(aBitmapEx, aRect);

    // This action is not creating line and fill, set directly, do not use SetAttributes(..)
    pGraf->SetMergedItem(XLineStyleItem(XLINE_NONE));
    pGraf->SetMergedItem(XFillStyleItem(drawing::FillStyle_NONE));
    InsertObj(pGraf);
}

void ImpSdrGDIMetaFileImport::DoAction(MetaMaskScalePartAction& rAct)
{
    Rectangle aRect(rAct.GetDestPoint(), rAct.GetDestSize());
    BitmapEx aBitmapEx(rAct.GetBitmap(), rAct.GetColor());

    aRect.Right()++; aRect.Bottom()++;
    aBitmapEx.Crop(Rectangle(rAct.GetSrcPoint(), rAct.GetSrcSize()));
    SdrGrafObj* pGraf = new SdrGrafObj(aBitmapEx, aRect);

    // This action is not creating line and fill, set directly, do not use SetAttributes(..)
    pGraf->SetMergedItem(XLineStyleItem(XLINE_NONE));
    pGraf->SetMergedItem(XFillStyleItem(drawing::FillStyle_NONE));
    InsertObj(pGraf);
}

namespace
{
    XGradientStyle getXGradientStyleFromGradientStyle(const GradientStyle& rGradientStyle)
    {
        XGradientStyle aXGradientStyle(XGRAD_LINEAR);

        switch(rGradientStyle)
        {
            case GradientStyle_LINEAR: aXGradientStyle = XGRAD_LINEAR; break;
            case GradientStyle_AXIAL: aXGradientStyle = XGRAD_AXIAL; break;
            case GradientStyle_RADIAL: aXGradientStyle = XGRAD_RADIAL; break;
            case GradientStyle_ELLIPTICAL: aXGradientStyle = XGRAD_ELLIPTICAL; break;
            case GradientStyle_SQUARE: aXGradientStyle = XGRAD_SQUARE; break;
            case GradientStyle_RECT: aXGradientStyle = XGRAD_RECT; break;

            // Needed due to GradientStyle_FORCE_EQUAL_SIZE; this again is needed
            // to force the enum defines in VCL to a defined size for the compilers,
            // so despite it is never used it cannot be removed (would break the
            // API implementation probably).
            case GradientStyle_FORCE_EQUAL_SIZE: break;
        }

        return aXGradientStyle;
    }
}

void ImpSdrGDIMetaFileImport::DoAction(MetaGradientAction& rAct)
{
    basegfx::B2DRange aRange(rAct.GetRect().Left(), rAct.GetRect().Top(), rAct.GetRect().Right(), rAct.GetRect().Bottom());

    if(!aRange.isEmpty())
    {
        const basegfx::B2DHomMatrix aTransform(basegfx::tools::createScaleTranslateB2DHomMatrix(mfScaleX, mfScaleY, maOfs.X(), maOfs.Y()));
        aRange.transform(aTransform);
        const Gradient& rGradient = rAct.GetGradient();
        SdrRectObj* pRect = new SdrRectObj(
            Rectangle(
                floor(aRange.getMinX()),
                floor(aRange.getMinY()),
                ceil(aRange.getMaxX()),
                ceil(aRange.getMaxY())));
        // #i125211# Use the ranges from the SdrObject to create a new empty SfxItemSet
        SfxItemSet aGradientAttr(mpModel->GetItemPool(), pRect->GetMergedItemSet().GetRanges());
        const XGradientStyle aXGradientStyle(getXGradientStyleFromGradientStyle(rGradient.GetStyle()));
        const XFillGradientItem aXFillGradientItem(
            XGradient(
                rGradient.GetStartColor(),
                rGradient.GetEndColor(),
                aXGradientStyle,
                rGradient.GetAngle(),
                rGradient.GetOfsX(),
                rGradient.GetOfsY(),
                rGradient.GetBorder(),
                rGradient.GetStartIntensity(),
                rGradient.GetEndIntensity(),
                rGradient.GetSteps()));

        SetAttributes(pRect);
        aGradientAttr.Put(XFillStyleItem(drawing::FillStyle_GRADIENT)); // #i125211#
        aGradientAttr.Put(aXFillGradientItem);
        pRect->SetMergedItemSet(aGradientAttr);

        InsertObj(pRect, false);
    }
}

void ImpSdrGDIMetaFileImport::DoAction(MetaWallpaperAction&)
{
    OSL_ENSURE(false, "Tried to construct SdrObject from MetaWallpaperAction: not supported (!)");
}

void ImpSdrGDIMetaFileImport::DoAction(MetaTransparentAction& rAct)
{
    basegfx::B2DPolyPolygon aSource(rAct.GetPolyPolygon().getB2DPolyPolygon());

    if(aSource.count())
    {
        const basegfx::B2DHomMatrix aTransform(basegfx::tools::createScaleTranslateB2DHomMatrix(mfScaleX, mfScaleY, maOfs.X(), maOfs.Y()));
        aSource.transform(aTransform);
        aSource.setClosed(true);

        SdrPathObj* pPath = new SdrPathObj(OBJ_POLY, aSource);
        SetAttributes(pPath);
        pPath->SetMergedItem(XFillTransparenceItem(rAct.GetTransparence()));
        InsertObj(pPath, false);
    }
}

void ImpSdrGDIMetaFileImport::DoAction(MetaEPSAction&)
{
    OSL_ENSURE(false, "Tried to construct SdrObject from MetaEPSAction: not supported (!)");
}

void ImpSdrGDIMetaFileImport::DoAction(MetaTextLineAction&)
{
    OSL_ENSURE(false, "Tried to construct SdrObject from MetaTextLineAction: not supported (!)");
}

void ImpSdrGDIMetaFileImport::DoAction(MetaGradientExAction& rAct)
{
    basegfx::B2DPolyPolygon aSource(rAct.GetPolyPolygon().getB2DPolyPolygon());

    if(aSource.count())
    {
        const basegfx::B2DHomMatrix aTransform(basegfx::tools::createScaleTranslateB2DHomMatrix(mfScaleX, mfScaleY, maOfs.X(), maOfs.Y()));
        aSource.transform(aTransform);

        if(!mbLastObjWasPolyWithoutLine || !CheckLastPolyLineAndFillMerge(aSource))
        {
            const Gradient& rGradient = rAct.GetGradient();
            SdrPathObj* pPath = new SdrPathObj(OBJ_POLY, aSource);
            // #i125211# Use the ranges from the SdrObject to create a new empty SfxItemSet
            SfxItemSet aGradientAttr(mpModel->GetItemPool(), pPath->GetMergedItemSet().GetRanges());
            const XGradientStyle aXGradientStyle(getXGradientStyleFromGradientStyle(rGradient.GetStyle()));
            const XFillGradientItem aXFillGradientItem(
                XGradient(
                    rGradient.GetStartColor(),
                    rGradient.GetEndColor(),
                    aXGradientStyle,
                    rGradient.GetAngle(),
                    rGradient.GetOfsX(),
                    rGradient.GetOfsY(),
                    rGradient.GetBorder(),
                    rGradient.GetStartIntensity(),
                    rGradient.GetEndIntensity(),
                    rGradient.GetSteps()));

            SetAttributes(pPath);
            aGradientAttr.Put(XFillStyleItem(drawing::FillStyle_GRADIENT)); // #i125211#
            aGradientAttr.Put(aXFillGradientItem);
            pPath->SetMergedItemSet(aGradientAttr);

            InsertObj(pPath, false);
        }
    }
}

void ImpSdrGDIMetaFileImport::DoAction(MetaFloatTransparentAction& rAct)
{
    const GDIMetaFile& rMtf = rAct.GetGDIMetaFile();

    if(rMtf.GetActionSize())
    {
        const Rectangle aRect(rAct.GetPoint(),rAct.GetSize());

        // convert metafile sub-content to BitmapEx
        BitmapEx aBitmapEx(
            convertMetafileToBitmapEx(
                rMtf,
                basegfx::B2DRange(
                    aRect.Left(), aRect.Top(),
                    aRect.Right(), aRect.Bottom()),
                125000));

        // handle colors
        const Gradient& rGradient = rAct.GetGradient();
        basegfx::BColor aStart(rGradient.GetStartColor().getBColor());
        basegfx::BColor aEnd(rGradient.GetEndColor().getBColor());

        if(100 != rGradient.GetStartIntensity())
        {
            aStart *= (double)rGradient.GetStartIntensity() / 100.0;
        }

        if(100 != rGradient.GetEndIntensity())
        {
            aEnd *= (double)rGradient.GetEndIntensity() / 100.0;
        }

        const bool bEqualColors(aStart == aEnd);
        const bool bNoSteps(1 == rGradient.GetSteps());
        bool bCreateObject(true);
        bool bHasNewMask(false);
        AlphaMask aNewMask;
        double fTransparence(0.0);
        bool bFixedTransparence(false);

        if(bEqualColors || bNoSteps)
        {
            // single transparence
            const basegfx::BColor aMedium(basegfx::average(aStart, aEnd));
            fTransparence = aMedium.luminance();

            if(basegfx::fTools::lessOrEqual(fTransparence, 0.0))
            {
                // no transparence needed, all done
            }
            else if(basegfx::fTools::moreOrEqual(fTransparence, 1.0))
            {
                // all transparent, no object
                bCreateObject = false;
            }
            else
            {
                // 0.0 < transparence < 1.0, apply fixed transparence
                bFixedTransparence = true;
            }
        }
        else
        {
            // gradient transparence
            VirtualDevice aVDev;

            aVDev.SetOutputSizePixel(aBitmapEx.GetBitmap().GetSizePixel());
            aVDev.DrawGradient(Rectangle(Point(0, 0), aVDev.GetOutputSizePixel()), rGradient);

            aNewMask = AlphaMask(aVDev.GetBitmap(Point(0, 0), aVDev.GetOutputSizePixel()));
            bHasNewMask = true;
        }

        if(bCreateObject)
        {
            if(bHasNewMask || bFixedTransparence)
            {
                if(!aBitmapEx.IsAlpha() && !aBitmapEx.IsTransparent())
                {
                    // no transparence yet, apply new one
                    if(bFixedTransparence)
                    {
                        sal_uInt8 aAlpha(basegfx::fround(fTransparence * 255.0));

                        aNewMask = AlphaMask(aBitmapEx.GetBitmap().GetSizePixel(), &aAlpha);
                    }

                    aBitmapEx = BitmapEx(aBitmapEx.GetBitmap(), aNewMask);
                }
                else
                {
                    // mix existing and new alpha mask
                    AlphaMask aOldMask;

                    if(aBitmapEx.IsAlpha())
                    {
                        aOldMask = aBitmapEx.GetAlpha();
                    }
                    else if(TRANSPARENT_BITMAP == aBitmapEx.GetTransparentType())
                    {
                        aOldMask = aBitmapEx.GetMask();
                    }
                    else if(TRANSPARENT_COLOR == aBitmapEx.GetTransparentType())
                    {
                        aOldMask = aBitmapEx.GetBitmap().CreateMask(aBitmapEx.GetTransparentColor());
                    }

                    BitmapWriteAccess* pOld = aOldMask.AcquireWriteAccess();

                    if(pOld)
                    {
                        const double fFactor(1.0 / 255.0);

                        if(bFixedTransparence)
                        {
                            const double fOpNew(1.0 - fTransparence);

                            for(int y(0); y < pOld->Height(); y++)
                            {
                                for(int x(0); x < pOld->Width(); x++)
                                {
                                    const double fOpOld(1.0 - (pOld->GetPixel(y, x).GetIndex() * fFactor));
                                    const sal_uInt8 aCol(basegfx::fround((1.0 - (fOpOld * fOpNew)) * 255.0));

                                    pOld->SetPixel(y, x, BitmapColor(aCol));
                                }
                            }
                        }
                        else
                        {
                            BitmapReadAccess* pNew = aNewMask.AcquireReadAccess();

                            if(pNew)
                            {
                                if(pOld->Width() == pNew->Width() && pOld->Height() == pNew->Height())
                                {
                                    for(int y(0); y < pOld->Height(); y++)
                                    {
                                        for(int x(0); x < pOld->Width(); x++)
                                        {
                                            const double fOpOld(1.0 - (pOld->GetPixel(y, x).GetIndex() * fFactor));
                                            const double fOpNew(1.0 - (pNew->GetPixel(y, x).GetIndex() * fFactor));
                                            const sal_uInt8 aCol(basegfx::fround((1.0 - (fOpOld * fOpNew)) * 255.0));

                                            pOld->SetPixel(y, x, BitmapColor(aCol));
                                        }
                                    }
                                }
                                else
                                {
                                    OSL_ENSURE(false, "Alpha masks have different sizes (!)");
                                }

                                aNewMask.ReleaseAccess(pNew);
                            }
                            else
                            {
                                OSL_ENSURE(false, "Got no access to new alpha mask (!)");
                            }
                        }

                        aOldMask.ReleaseAccess(pOld);
                    }
                    else
                    {
                        OSL_ENSURE(false, "Got no access to old alpha mask (!)");
                    }

                    // apply combined bitmap as mask
                    aBitmapEx = BitmapEx(aBitmapEx.GetBitmap(), aOldMask);
                }
            }

            // create and add object
            SdrGrafObj* pGraf = new SdrGrafObj(aBitmapEx, aRect);

            // for MetaFloatTransparentAction, do not use SetAttributes(...)
            // since these metafile content is not used to draw line/fill
            // dependent of these setting at the device content
            pGraf->SetMergedItem(XLineStyleItem(XLINE_NONE));
            pGraf->SetMergedItem(XFillStyleItem(drawing::FillStyle_NONE));
            InsertObj(pGraf);
        }
    }
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
