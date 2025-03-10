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
#include <editeng/eeitem.hxx>

#include <tools/urlobj.hxx>
#include <editeng/editobj.hxx>
#include <editeng/editstat.hxx>
#include <editeng/frmdiritem.hxx>
#include <editeng/langitem.hxx>
#include <sfx2/linkmgr.hxx>
#include <editeng/scripttypeitem.hxx>
#include <editeng/unolingu.hxx>
#include <sfx2/bindings.hxx>
#include <sfx2/objsh.hxx>
#include <sfx2/printer.hxx>
#include <sfx2/viewfrm.hxx>
#include <sfx2/viewsh.hxx>
#include <svl/flagitem.hxx>
#include <svl/intitem.hxx>
#include <svl/zforlist.hxx>
#include <svl/zformat.hxx>
#include <unotools/misccfg.hxx>
#include <sfx2/app.hxx>
#include <unotools/transliterationwrapper.hxx>
#include <unotools/securityoptions.hxx>

#include <vcl/virdev.hxx>
#include <vcl/msgbox.hxx>

#include <com/sun/star/i18n/TransliterationModulesExtra.hpp>

#include "inputopt.hxx"
#include "global.hxx"
#include "table.hxx"
#include "column.hxx"
#include "poolhelp.hxx"
#include "docpool.hxx"
#include "stlpool.hxx"
#include "stlsheet.hxx"
#include "docoptio.hxx"
#include "viewopti.hxx"
#include "scextopt.hxx"
#include "rechead.hxx"
#include "ddelink.hxx"
#include "scmatrix.hxx"
#include "arealink.hxx"
#include "dociter.hxx"
#include "patattr.hxx"
#include "hints.hxx"
#include "editutil.hxx"
#include "progress.hxx"
#include "document.hxx"
#include "chartlis.hxx"
#include "chartlock.hxx"
#include "refupdat.hxx"
#include "validat.hxx"
#include "markdata.hxx"
#include "scmod.hxx"
#include "printopt.hxx"
#include "externalrefmgr.hxx"
#include "globstr.hrc"
#include "sc.hrc"
#include "charthelper.hxx"
#include "macromgr.hxx"
#include "dpobject.hxx"
#include "docuno.hxx"
#include "scresid.hxx"
#include "columniterator.hxx"
#include "globalnames.hxx"
#include "stringutil.hxx"
#include <documentlinkmgr.hxx>
#include <scopetools.hxx>

#include <boost/scoped_ptr.hpp>

using namespace com::sun::star;

// STATIC DATA -----------------------------------------------------------

namespace {

inline sal_uInt16 getScaleValue(SfxStyleSheetBase& rStyle, sal_uInt16 nWhich)
{
    return static_cast<const SfxUInt16Item&>(rStyle.GetItemSet().Get(nWhich)).GetValue();
}

}

void ScDocument::ImplCreateOptions()
{
    pDocOptions  = new ScDocOptions();
    pViewOptions = new ScViewOptions();
}

void ScDocument::ImplDeleteOptions()
{
    delete pDocOptions;
    delete pViewOptions;
    delete pExtDocOptions;
}

SfxPrinter* ScDocument::GetPrinter(bool bCreateIfNotExist)
{
    if ( !pPrinter && bCreateIfNotExist )
    {
        SfxItemSet* pSet =
            new SfxItemSet( *xPoolHelper->GetDocPool(),
                            SID_PRINTER_NOTFOUND_WARN,  SID_PRINTER_NOTFOUND_WARN,
                            SID_PRINTER_CHANGESTODOC,   SID_PRINTER_CHANGESTODOC,
                            SID_PRINT_SELECTEDSHEET,    SID_PRINT_SELECTEDSHEET,
                            SID_SCPRINTOPTIONS,         SID_SCPRINTOPTIONS,
                            NULL );

        ::utl::MiscCfg aMisc;
        sal_uInt16 nFlags = 0;
        if ( aMisc.IsPaperOrientationWarning() )
            nFlags |= SFX_PRINTER_CHG_ORIENTATION;
        if ( aMisc.IsPaperSizeWarning() )
            nFlags |= SFX_PRINTER_CHG_SIZE;
        pSet->Put( SfxFlagItem( SID_PRINTER_CHANGESTODOC, nFlags ) );
        pSet->Put( SfxBoolItem( SID_PRINTER_NOTFOUND_WARN, aMisc.IsNotFoundWarning() ) );

        pPrinter = new SfxPrinter( pSet );
        pPrinter->SetMapMode( MAP_100TH_MM );
        UpdateDrawPrinter();
        pPrinter->SetDigitLanguage( SC_MOD()->GetOptDigitLanguage() );
    }

    return pPrinter;
}

void ScDocument::SetPrinter( SfxPrinter* pNewPrinter )
{
    if ( pNewPrinter == pPrinter )
    {
        //  #i6706# SetPrinter is called with the same printer again if
        //  the JobSetup has changed. In that case just call UpdateDrawPrinter
        //  (SetRefDevice for drawing layer) because of changed text sizes.
        UpdateDrawPrinter();
    }
    else
    {
        SfxPrinter* pOld = pPrinter;
        pPrinter = pNewPrinter;
        UpdateDrawPrinter();
        pPrinter->SetDigitLanguage( SC_MOD()->GetOptDigitLanguage() );
        delete pOld;
    }
    InvalidateTextWidth(NULL, NULL, false);     // in both cases
}

void ScDocument::SetPrintOptions()
{
    if ( !pPrinter ) GetPrinter(); // setzt pPrinter
    OSL_ENSURE( pPrinter, "Error in printer creation :-/" );

    if ( pPrinter )
    {
        ::utl::MiscCfg aMisc;
        SfxItemSet aOptSet( pPrinter->GetOptions() );

        sal_uInt16 nFlags = 0;
        if ( aMisc.IsPaperOrientationWarning() )
            nFlags |= SFX_PRINTER_CHG_ORIENTATION;
        if ( aMisc.IsPaperSizeWarning() )
            nFlags |= SFX_PRINTER_CHG_SIZE;
        aOptSet.Put( SfxFlagItem( SID_PRINTER_CHANGESTODOC, nFlags ) );
        aOptSet.Put( SfxBoolItem( SID_PRINTER_NOTFOUND_WARN, aMisc.IsNotFoundWarning() ) );

        pPrinter->SetOptions( aOptSet );
    }
}

VirtualDevice* ScDocument::GetVirtualDevice_100th_mm()
{
    if (!pVirtualDevice_100th_mm)
    {
#ifdef IOS
        pVirtualDevice_100th_mm = new VirtualDevice( 8 );
#else
        pVirtualDevice_100th_mm = new VirtualDevice( 1 );
#endif
        pVirtualDevice_100th_mm->SetReferenceDevice(VirtualDevice::REFDEV_MODE_MSO1);
        MapMode aMapMode( pVirtualDevice_100th_mm->GetMapMode() );
        aMapMode.SetMapUnit( MAP_100TH_MM );
        pVirtualDevice_100th_mm->SetMapMode( aMapMode );
    }
    return pVirtualDevice_100th_mm;
}

OutputDevice* ScDocument::GetRefDevice()
{
    // Create printer like ref device, see Writer...
    OutputDevice* pRefDevice = NULL;
    if ( SC_MOD()->GetInputOptions().GetTextWysiwyg() )
        pRefDevice = GetPrinter();
    else
        pRefDevice = GetVirtualDevice_100th_mm();
    return pRefDevice;
}

void ScDocument::ModifyStyleSheet( SfxStyleSheetBase& rStyleSheet,
                                   const SfxItemSet&  rChanges )
{
    SfxItemSet& rSet = rStyleSheet.GetItemSet();

    switch ( rStyleSheet.GetFamily() )
    {
        case SFX_STYLE_FAMILY_PAGE:
            {
                const sal_uInt16 nOldScale = getScaleValue(rStyleSheet, ATTR_PAGE_SCALE);
                const sal_uInt16 nOldScaleToPages = getScaleValue(rStyleSheet, ATTR_PAGE_SCALETOPAGES);
                rSet.Put( rChanges );
                const sal_uInt16 nNewScale        = getScaleValue(rStyleSheet, ATTR_PAGE_SCALE);
                const sal_uInt16 nNewScaleToPages = getScaleValue(rStyleSheet, ATTR_PAGE_SCALETOPAGES);

                if ( (nOldScale != nNewScale) || (nOldScaleToPages != nNewScaleToPages) )
                    InvalidateTextWidth( rStyleSheet.GetName() );

                if( SvtLanguageOptions().IsCTLFontEnabled() )
                {
                    const SfxPoolItem *pItem = NULL;
                    if( rChanges.GetItemState(ATTR_WRITINGDIR, true, &pItem ) == SFX_ITEM_SET )
                        ScChartHelper::DoUpdateAllCharts( this );
                }
            }
            break;

        case SFX_STYLE_FAMILY_PARA:
            {
                bool bNumFormatChanged;
                if ( ScGlobal::CheckWidthInvalidate( bNumFormatChanged,
                        rSet, rChanges ) )
                    InvalidateTextWidth( NULL, NULL, bNumFormatChanged );

                for (SCTAB nTab=0; nTab<=MAXTAB; ++nTab)
                    if (maTabs[nTab] && maTabs[nTab]->IsStreamValid())
                        maTabs[nTab]->SetStreamValid( false );

                sal_uLong nOldFormat =
                    ((const SfxUInt32Item*)&rSet.Get(
                    ATTR_VALUE_FORMAT ))->GetValue();
                sal_uLong nNewFormat =
                    ((const SfxUInt32Item*)&rChanges.Get(
                    ATTR_VALUE_FORMAT ))->GetValue();
                LanguageType eNewLang, eOldLang;
                eNewLang = eOldLang = LANGUAGE_DONTKNOW;
                if ( nNewFormat != nOldFormat )
                {
                    SvNumberFormatter* pFormatter = GetFormatTable();
                    eOldLang = pFormatter->GetEntry( nOldFormat )->GetLanguage();
                    eNewLang = pFormatter->GetEntry( nNewFormat )->GetLanguage();
                }

                // Bedeutung der Items in rChanges:
                //  Item gesetzt    - Aenderung uebernehmen
                //  Dontcare        - Default setzen
                //  Default         - keine Aenderung
                // ("keine Aenderung" geht nicht mit PutExtended, darum Schleife)
                for (sal_uInt16 nWhich = ATTR_PATTERN_START; nWhich <= ATTR_PATTERN_END; nWhich++)
                {
                    const SfxPoolItem* pItem;
                    SfxItemState eState = rChanges.GetItemState( nWhich, false, &pItem );
                    if ( eState == SFX_ITEM_SET )
                        rSet.Put( *pItem );
                    else if ( eState == SFX_ITEM_DONTCARE )
                        rSet.ClearItem( nWhich );
                    // bei Default nichts
                }

                if ( eNewLang != eOldLang )
                    rSet.Put(
                        SvxLanguageItem( eNewLang, ATTR_LANGUAGE_FORMAT ) );
            }
            break;
        default:
        {
            // added to avoid warnings
        }
    }
}

void ScDocument::CopyStdStylesFrom( ScDocument* pSrcDoc )
{
    // number format exchange list has to be handled here, too
    NumFmtMergeHandler aNumFmtMergeHdl(this, pSrcDoc);
    xPoolHelper->GetStylePool()->CopyStdStylesFrom( pSrcDoc->xPoolHelper->GetStylePool() );
}

void ScDocument::InvalidateTextWidth( const OUString& rStyleName )
{
    const SCTAB nCount = GetTableCount();
    for ( SCTAB i=0; i<nCount && maTabs[i]; i++ )
        if ( maTabs[i]->GetPageStyle() == rStyleName )
            InvalidateTextWidth( i );
}

void ScDocument::InvalidateTextWidth( SCTAB nTab )
{
    ScAddress aAdrFrom( 0,    0,        nTab );
    ScAddress aAdrTo  ( MAXCOL, MAXROW, nTab );
    InvalidateTextWidth( &aAdrFrom, &aAdrTo, false );
}

bool ScDocument::IsPageStyleInUse( const OUString& rStrPageStyle, SCTAB* pInTab )
{
    bool         bInUse = false;
    const SCTAB nCount = GetTableCount();
    SCTAB i;

    for ( i = 0; !bInUse && i < nCount && maTabs[i]; i++ )
        bInUse = ( maTabs[i]->GetPageStyle() == rStrPageStyle );

    if ( pInTab )
        *pInTab = i-1;

    return bInUse;
}

bool ScDocument::RemovePageStyleInUse( const OUString& rStyle )
{
    bool bWasInUse = false;
    const SCTAB nCount = GetTableCount();

    for ( SCTAB i=0; i<nCount && maTabs[i]; i++ )
        if ( maTabs[i]->GetPageStyle() == rStyle )
        {
            bWasInUse = true;
            maTabs[i]->SetPageStyle( ScGlobal::GetRscString(STR_STYLENAME_STANDARD) );
        }

    return bWasInUse;
}

bool ScDocument::RenamePageStyleInUse( const OUString& rOld, const OUString& rNew )
{
    bool bWasInUse = false;
    const SCTAB nCount = GetTableCount();

    for ( SCTAB i=0; i<nCount && maTabs[i]; i++ )
        if ( maTabs[i]->GetPageStyle() == rOld )
        {
            bWasInUse = true;
            maTabs[i]->SetPageStyle( rNew );
        }

    return bWasInUse;
}

sal_uInt8 ScDocument::GetEditTextDirection(SCTAB nTab) const
{
    EEHorizontalTextDirection eRet = EE_HTEXTDIR_DEFAULT;

    OUString aStyleName = GetPageStyle( nTab );
    SfxStyleSheetBase* pStyle = xPoolHelper->GetStylePool()->Find( aStyleName, SFX_STYLE_FAMILY_PAGE );
    if ( pStyle )
    {
        SfxItemSet& rStyleSet = pStyle->GetItemSet();
        SvxFrameDirection eDirection = (SvxFrameDirection)
            ((const SvxFrameDirectionItem&)rStyleSet.Get( ATTR_WRITINGDIR )).GetValue();

        if ( eDirection == FRMDIR_HORI_LEFT_TOP )
            eRet = EE_HTEXTDIR_L2R;
        else if ( eDirection == FRMDIR_HORI_RIGHT_TOP )
            eRet = EE_HTEXTDIR_R2L;
        // else (invalid for EditEngine): keep "default"
    }

    return sal::static_int_cast<sal_uInt8>(eRet);
}

ScMacroManager* ScDocument::GetMacroManager()
{
    if (!mpMacroMgr.get())
        mpMacroMgr.reset(new ScMacroManager(this));
    return mpMacroMgr.get();
}

void ScDocument::FillMatrix(
    ScMatrix& rMat, SCTAB nTab, SCCOL nCol1, SCROW nRow1, SCCOL nCol2, SCROW nRow2 ) const
{
    const ScTable* pTab = FetchTable(nTab);
    if (!pTab)
        return;

    if (nCol1 > nCol2 || nRow1 > nRow2)
        return;

    SCSIZE nC, nR;
    rMat.GetDimensions(nC, nR);
    if (static_cast<SCROW>(nR) != nRow2 - nRow1 + 1 || static_cast<SCCOL>(nC) != nCol2 - nCol1 + 1)
        return;

    pTab->FillMatrix(rMat, nCol1, nRow1, nCol2, nRow2);
}

void ScDocument::SetFormulaResults( const ScAddress& rTopPos, const double* pResults, size_t nLen )
{
    ScTable* pTab = FetchTable(rTopPos.Tab());
    if (!pTab)
        return;

    pTab->SetFormulaResults(rTopPos.Col(), rTopPos.Row(), pResults, nLen);
}

void ScDocument::SetFormulaResults(
    const ScAddress& rTopPos, const formula::FormulaTokenRef* pResults, size_t nLen )
{
    ScTable* pTab = FetchTable(rTopPos.Tab());
    if (!pTab)
        return;

    pTab->SetFormulaResults(rTopPos.Col(), rTopPos.Row(), pResults, nLen);
}

void ScDocument::InvalidateTextWidth( const ScAddress* pAdrFrom, const ScAddress* pAdrTo,
                                      bool bNumFormatChanged )
{
    bool bBroadcast = (bNumFormatChanged && GetDocOptions().IsCalcAsShown() && !IsImportingXML() && !IsClipboard());
    if ( pAdrFrom && !pAdrTo )
    {
        const SCTAB nTab = pAdrFrom->Tab();

        if (nTab < static_cast<SCTAB>(maTabs.size()) && maTabs[nTab] )
            maTabs[nTab]->InvalidateTextWidth( pAdrFrom, NULL, bNumFormatChanged, bBroadcast );
    }
    else
    {
        const SCTAB nTabStart = pAdrFrom ? pAdrFrom->Tab() : 0;
        const SCTAB nTabEnd   = pAdrTo   ? pAdrTo->Tab()   : MAXTAB;

        for ( SCTAB nTab=nTabStart; nTab<=nTabEnd && nTab < static_cast<SCTAB>(maTabs.size()); nTab++ )
            if ( maTabs[nTab] )
                maTabs[nTab]->InvalidateTextWidth( pAdrFrom, pAdrTo, bNumFormatChanged, bBroadcast );
    }
}

#define CALCMAX                 1000    // Berechnungen
#define ABORT_EVENTS            (VCL_INPUT_ANY & ~VCL_INPUT_TIMER & ~VCL_INPUT_OTHER)

namespace {

class IdleCalcTextWidthScope
{
    ScDocument& mrDoc;
    ScAddress& mrCalcPos;
    MapMode maOldMapMode;
    sal_uLong mnStartTime;
    ScStyleSheetPool* mpStylePool;
    sal_uInt16 mnOldSearchMask;
    SfxStyleFamily meOldFamily;
    bool mbNeedMore;
    bool mbProgress;

public:
    IdleCalcTextWidthScope(ScDocument& rDoc, ScAddress& rCalcPos) :
        mrDoc(rDoc),
        mrCalcPos(rCalcPos),
        mnStartTime(Time::GetSystemTicks()),
        mpStylePool(rDoc.GetStyleSheetPool()),
        mnOldSearchMask(mpStylePool->GetSearchMask()),
        meOldFamily(mpStylePool->GetSearchFamily()),
        mbNeedMore(false),
        mbProgress(false)
    {
        // The old search mask / family flags must be restored so that e.g.
        // the styles dialog shows correct listing when it's opened in-between
        // the calls.

        mrDoc.EnableIdle(false);
        mpStylePool->SetSearchMask(SFX_STYLE_FAMILY_PAGE, SFXSTYLEBIT_ALL);
    }

    ~IdleCalcTextWidthScope()
    {
        SfxPrinter* pDev = mrDoc.GetPrinter();
        if (pDev)
            pDev->SetMapMode(maOldMapMode);

        if (mbProgress)
            ScProgress::DeleteInterpretProgress();

        mpStylePool->SetSearchMask(meOldFamily, mnOldSearchMask);
        mrDoc.EnableIdle(true);
    }

    SCTAB Tab() const { return mrCalcPos.Tab(); }
    SCCOL Col() const { return mrCalcPos.Col(); }
    SCROW Row() const { return mrCalcPos.Row(); }

    void setTab(SCTAB nTab) { mrCalcPos.SetTab(nTab); }
    void setCol(SCCOL nCol) { mrCalcPos.SetCol(nCol); }
    void setRow(SCROW nRow) { mrCalcPos.SetRow(nRow); }

    void incTab(SCTAB nInc=1) { mrCalcPos.IncTab(nInc); }
    void incCol(SCCOL nInc=1) { mrCalcPos.IncCol(nInc); }

    void setOldMapMode(const MapMode& rOldMapMode) { maOldMapMode = rOldMapMode; }

    void setNeedMore(bool b) { mbNeedMore = b; }
    bool getNeedMore() const { return mbNeedMore; }

    sal_uLong getStartTime() const { return mnStartTime; }

    void createProgressBar()
    {
        ScProgress::CreateInterpretProgress(&mrDoc, false);
        mbProgress = true;
    }

    bool hasProgressBar() const { return mbProgress; }

    ScStyleSheetPool* getStylePool() { return mpStylePool; }
};

}

bool ScDocument::IdleCalcTextWidth()            // true = demnaechst wieder versuchen
{
    // #i75610# if a printer hasn't been set or created yet, don't create one for this
    if (!mbIdleEnabled || IsInLinkUpdate() || GetPrinter(false) == NULL)
        return false;

    IdleCalcTextWidthScope aScope(*this, aCurTextWidthCalcPos);

    if (!ValidRow(aScope.Row()))
    {
        aScope.setRow(0);
        aScope.incCol(-1);
    }

    if (aScope.Col() < 0)
    {
        aScope.setCol(MAXCOL);
        aScope.incTab();
    }

    if (!ValidTab(aScope.Tab()) || aScope.Tab() >= static_cast<SCTAB>(maTabs.size()) || !maTabs[aScope.Tab()])
        aScope.setTab(0);

    ScTable* pTab = maTabs[aScope.Tab()];
    ScStyleSheet* pStyle = (ScStyleSheet*)aScope.getStylePool()->Find(pTab->aPageStyle, SFX_STYLE_FAMILY_PAGE);
    OSL_ENSURE( pStyle, "Missing StyleSheet :-/" );

    if (!pStyle || getScaleValue(*pStyle, ATTR_PAGE_SCALETOPAGES) == 0)
    {
        // Move to the next sheet as the current one has scale-to-pages set,
        // and bail out.
        aScope.incTab();
        return false;
    }

    sal_uInt16 nZoom = getScaleValue(*pStyle, ATTR_PAGE_SCALE);
    Fraction aZoomFract(nZoom, 100);

    // Start at specified cell position (nCol, nRow, nTab).
    ScColumn* pCol  = &pTab->aCol[aScope.Col()];
    boost::scoped_ptr<ScColumnTextWidthIterator> pColIter(new ScColumnTextWidthIterator(*pCol, aScope.Row(), MAXROW));

    OutputDevice* pDev = NULL;
    sal_uInt16 nRestart = 0;
    sal_uInt16 nCount = 0;
    while ( (nZoom > 0) && (nCount < CALCMAX) && (nRestart < 2) )
    {
        if (pColIter->hasCell())
        {
            // More cell in this column.
            SCROW nRow = pColIter->getPos();
            aScope.setRow(nRow);

            if (pColIter->getValue() == TEXTWIDTH_DIRTY)
            {
                // Calculate text width for this cell.
                double nPPTX = 0.0;
                double nPPTY = 0.0;
                if (!pDev)
                {
                    pDev = GetPrinter();
                    aScope.setOldMapMode(pDev->GetMapMode());
                    pDev->SetMapMode( MAP_PIXEL );  // wichtig fuer GetNeededSize

                    Point aPix1000 = pDev->LogicToPixel( Point(1000,1000), MAP_TWIP );
                    nPPTX = aPix1000.X() / 1000.0;
                    nPPTY = aPix1000.Y() / 1000.0;
                }

                if (!aScope.hasProgressBar() && pCol->IsFormulaDirty(nRow))
                    aScope.createProgressBar();

                sal_uInt16 nNewWidth = (sal_uInt16)GetNeededSize(
                    aScope.Col(), aScope.Row(), aScope.Tab(),
                    pDev, nPPTX, nPPTY, aZoomFract,aZoomFract, true, true);   // bTotalSize

                pColIter->setValue(nNewWidth);
                aScope.setNeedMore(true);
            }
            pColIter->next();
        }
        else
        {
            // No more cell in this column.  Move to the left column and start at row 0.

            bool bNewTab = false;

            aScope.setRow(0);
            aScope.incCol(-1);

            if (aScope.Col() < 0)
            {
                // No more column to the left.  Move to the right-most column of the next sheet.
                aScope.setCol(MAXCOL);
                aScope.incTab();
                bNewTab = true;
            }

            if (!ValidTab(aScope.Tab()) || aScope.Tab() >= static_cast<SCTAB>(maTabs.size()) || !maTabs[aScope.Tab()] )
            {
                // Sheet doesn't exist at specified sheet position.  Restart at sheet 0.
                aScope.setTab(0);
                nRestart++;
                bNewTab = true;
            }

            if ( nRestart < 2 )
            {
                if ( bNewTab )
                {
                    pTab = maTabs[aScope.Tab()];
                    pStyle = (ScStyleSheet*)aScope.getStylePool()->Find(
                        pTab->aPageStyle, SFX_STYLE_FAMILY_PAGE);

                    if ( pStyle )
                    {
                        // Check if the scale-to-pages setting is set. If
                        // set, we exit the loop.  If not, get the page
                        // scale factor of the new sheet.
                        if (getScaleValue(*pStyle, ATTR_PAGE_SCALETOPAGES) == 0)
                        {
                            nZoom = getScaleValue(*pStyle, ATTR_PAGE_SCALE);
                            aZoomFract = Fraction(nZoom, 100);
                        }
                        else
                            nZoom = 0;
                    }
                    else
                    {
                        OSL_FAIL( "Missing StyleSheet :-/" );
                    }
                }

                if ( nZoom > 0 )
                {
                    pCol  = &pTab->aCol[aScope.Col()];
                    pColIter.reset(new ScColumnTextWidthIterator(*pCol, aScope.Row(), MAXROW));
                }
                else
                {
                    aScope.incTab(); // Move to the next sheet as the current one has scale-to-pages set.
                    return false;
                }
            }
        }

        ++nCount;

        // Quit if either 1) its duration exceeds 50 ms, or 2) there is any
        // pending event after processing 32 cells.
        if ((50L < Time::GetSystemTicks() - aScope.getStartTime()) || (nCount > 31 && Application::AnyInput(ABORT_EVENTS)))
            nCount = CALCMAX;
    }

    return aScope.getNeedMore();
}

void ScDocument::RepaintRange( const ScRange& rRange )
{
    if ( bIsVisible && pShell )
    {
        ScModelObj* pModel = ScModelObj::getImplementation( pShell->GetModel() );
        if ( pModel )
            pModel->RepaintRange( rRange );     // locked repaints are checked there
    }
}

void ScDocument::RepaintRange( const ScRangeList& rRange )
{
    if ( bIsVisible && pShell )
    {
        ScModelObj* pModel = ScModelObj::getImplementation( pShell->GetModel() );
        if ( pModel )
            pModel->RepaintRange( rRange );     // locked repaints are checked there
    }
}

void ScDocument::SaveDdeLinks(SvStream& rStream) const
{
    //  bei 4.0-Export alle mit Modus != DEFAULT weglassen
    bool bExport40 = ( rStream.GetVersion() <= SOFFICE_FILEFORMAT_40 );

    const ::sfx2::SvBaseLinks& rLinks = GetLinkManager()->GetLinks();
    sal_uInt16 nCount = rLinks.size();

    //  erstmal zaehlen...

    sal_uInt16 nDdeCount = 0;
    sal_uInt16 i;
    for (i=0; i<nCount; i++)
    {
        ::sfx2::SvBaseLink* pBase = *rLinks[i];
        if (pBase->ISA(ScDdeLink))
            if ( !bExport40 || ((ScDdeLink*)pBase)->GetMode() == SC_DDE_DEFAULT )
                ++nDdeCount;
    }

    //  Header

    ScMultipleWriteHeader aHdr( rStream );
    rStream.WriteUInt16( nDdeCount );

    //  Links speichern

    for (i=0; i<nCount; i++)
    {
        ::sfx2::SvBaseLink* pBase = *rLinks[i];
        if (pBase->ISA(ScDdeLink))
        {
            ScDdeLink* pLink = (ScDdeLink*)pBase;
            if ( !bExport40 || pLink->GetMode() == SC_DDE_DEFAULT )
                pLink->Store( rStream, aHdr );
        }
    }
}

void ScDocument::LoadDdeLinks(SvStream& rStream)
{
    sfx2::LinkManager* pMgr = GetDocLinkManager().getLinkManager(bAutoCalc);
    if (!pMgr)
        return;

    ScMultipleReadHeader aHdr( rStream );

    sal_uInt16 nCount;
    rStream.ReadUInt16( nCount );
    for (sal_uInt16 i=0; i<nCount; i++)
    {
        ScDdeLink* pLink = new ScDdeLink( this, rStream, aHdr );
        pMgr->InsertDDELink(pLink, pLink->GetAppl(), pLink->GetTopic(), pLink->GetItem());
    }
}

void ScDocument::SetInLinkUpdate(bool bSet)
{
    //  called from TableLink and AreaLink

    OSL_ENSURE( bInLinkUpdate != bSet, "SetInLinkUpdate twice" );
    bInLinkUpdate = bSet;
}

bool ScDocument::IsInLinkUpdate() const
{
    return bInLinkUpdate || IsInDdeLinkUpdate();
}

void ScDocument::UpdateExternalRefLinks(Window* pWin)
{
    if (!pExternalRefMgr.get())
        return;

    sfx2::LinkManager* pMgr = GetDocLinkManager().getLinkManager(bAutoCalc);
    if (!pMgr)
        return;

    const ::sfx2::SvBaseLinks& rLinks = pMgr->GetLinks();
    sal_uInt16 nCount = rLinks.size();

    bool bAny = false;

    // Collect all the external ref links first.
    std::vector<ScExternalRefLink*> aRefLinks;
    for (sal_uInt16 i = 0; i < nCount; ++i)
    {
        ::sfx2::SvBaseLink* pBase = *rLinks[i];
        ScExternalRefLink* pRefLink = dynamic_cast<ScExternalRefLink*>(pBase);
        if (pRefLink)
            aRefLinks.push_back(pRefLink);
    }

    sc::WaitPointerSwitch aWaitSwitch(pWin);

    pExternalRefMgr->enableDocTimer(false);
    ScProgress aProgress(GetDocumentShell(), ScResId(SCSTR_UPDATE_EXTDOCS).toString(), aRefLinks.size());
    for (size_t i = 0, n = aRefLinks.size(); i < n; ++i)
    {
        aProgress.SetState(i+1);

        ScExternalRefLink* pRefLink = aRefLinks[i];
        if (pRefLink->Update())
        {
            bAny = true;
            continue;
        }

        // Update failed.  Notify the user.

        OUString aFile;
        pMgr->GetDisplayNames(pRefLink, NULL, &aFile, NULL, NULL);
        // Decode encoded URL for display friendliness.
        INetURLObject aUrl(aFile,INetURLObject::WAS_ENCODED);
        aFile = aUrl.GetMainURL(INetURLObject::DECODE_UNAMBIGUOUS);

        OUStringBuffer aBuf;
        aBuf.append(OUString(ScResId(SCSTR_EXTDOC_NOT_LOADED)));
        aBuf.appendAscii("\n\n");
        aBuf.append(aFile);
        ErrorBox aBox(pWin, WB_OK, aBuf.makeStringAndClear());
        aBox.Execute();
    }

    pExternalRefMgr->enableDocTimer(true);

    if (bAny)
    {
        TrackFormulas();
        pShell->Broadcast( SfxSimpleHint(FID_DATACHANGED) );

        // #i101960# set document modified, as in TrackTimeHdl for DDE links
        if (!pShell->IsModified())
        {
            pShell->SetModified( true );
            SfxBindings* pBindings = GetViewBindings();
            if (pBindings)
            {
                pBindings->Invalidate( SID_SAVEDOC );
                pBindings->Invalidate( SID_DOC_MODIFIED );
            }
        }
    }
}

void ScDocument::CopyDdeLinks( ScDocument* pDestDoc ) const
{
    if (bIsClip)        // aus Stream erzeugen
    {
        if (pClipData)
        {
            pClipData->Seek(0);
            pDestDoc->LoadDdeLinks(*pClipData);
        }

        return;
    }

    const sfx2::LinkManager* pMgr = GetDocLinkManager().getExistingLinkManager();
    if (!pMgr)
        return;

    sfx2::LinkManager* pDestMgr = pDestDoc->GetDocLinkManager().getLinkManager(pDestDoc->bAutoCalc);
    if (!pDestMgr)
        return;

    const sfx2::SvBaseLinks& rLinks = pMgr->GetLinks();
    for (size_t i = 0, n = rLinks.size(); i < n; ++i)
    {
        const sfx2::SvBaseLink* pBase = *rLinks[i];
        if (pBase->ISA(ScDdeLink))
        {
            const ScDdeLink* p = static_cast<const ScDdeLink*>(pBase);
            ScDdeLink* pNew = new ScDdeLink(pDestDoc, *p);
            pDestMgr->InsertDDELink(
                pNew, pNew->GetAppl(), pNew->GetTopic(), pNew->GetItem());
        }
    }
}

namespace {

/** Tries to find the specified DDE link.
    @param pnDdePos  (out-param) if not 0, the index of the DDE link is returned here
                     (does not include other links from link manager).
    @return  The DDE link, if it exists, otherwise 0. */
ScDdeLink* lclGetDdeLink(
        const sfx2::LinkManager* pLinkManager,
        const OUString& rAppl, const OUString& rTopic, const OUString& rItem, sal_uInt8 nMode,
        size_t* pnDdePos = NULL )
{
    if( pLinkManager )
    {
        const ::sfx2::SvBaseLinks& rLinks = pLinkManager->GetLinks();
        size_t nCount = rLinks.size();
        if( pnDdePos ) *pnDdePos = 0;
        for( size_t nIndex = 0; nIndex < nCount; ++nIndex )
        {
            ::sfx2::SvBaseLink* pLink = *rLinks[ nIndex ];
            if( ScDdeLink* pDdeLink = PTR_CAST( ScDdeLink, pLink ) )
            {
                if( (OUString(pDdeLink->GetAppl()) == rAppl) &&
                    (OUString(pDdeLink->GetTopic()) == rTopic) &&
                    (OUString(pDdeLink->GetItem()) == rItem) &&
                    ((nMode == SC_DDE_IGNOREMODE) || (nMode == pDdeLink->GetMode())) )
                    return pDdeLink;
                if( pnDdePos ) ++*pnDdePos;
            }
        }
    }
    return NULL;
}

/** Returns a pointer to the specified DDE link.
    @param nDdePos  Index of the DDE link (does not include other links from link manager).
    @return  The DDE link, if it exists, otherwise 0. */
ScDdeLink* lclGetDdeLink( const sfx2::LinkManager* pLinkManager, size_t nDdePos )
{
    if( pLinkManager )
    {
        const ::sfx2::SvBaseLinks& rLinks = pLinkManager->GetLinks();
        size_t nCount = rLinks.size();
        size_t nDdeIndex = 0;       // counts only the DDE links
        for( size_t nIndex = 0; nIndex < nCount; ++nIndex )
        {
            ::sfx2::SvBaseLink* pLink = *rLinks[ nIndex ];
            if( ScDdeLink* pDdeLink = PTR_CAST( ScDdeLink, pLink ) )
            {
                if( nDdeIndex == nDdePos )
                    return pDdeLink;
                ++nDdeIndex;
            }
        }
    }
    return NULL;
}

} // namespace

bool ScDocument::FindDdeLink( const OUString& rAppl, const OUString& rTopic, const OUString& rItem,
        sal_uInt8 nMode, size_t& rnDdePos )
{
    return lclGetDdeLink( GetLinkManager(), rAppl, rTopic, rItem, nMode, &rnDdePos ) != NULL;
}

bool ScDocument::GetDdeLinkData( size_t nDdePos, OUString& rAppl, OUString& rTopic, OUString& rItem ) const
{
    if( const ScDdeLink* pDdeLink = lclGetDdeLink( GetLinkManager(), nDdePos ) )
    {
        rAppl  = pDdeLink->GetAppl();
        rTopic = pDdeLink->GetTopic();
        rItem  = pDdeLink->GetItem();
        return true;
    }
    return false;
}

bool ScDocument::GetDdeLinkMode( size_t nDdePos, sal_uInt8& rnMode ) const
{
    if( const ScDdeLink* pDdeLink = lclGetDdeLink( GetLinkManager(), nDdePos ) )
    {
        rnMode = pDdeLink->GetMode();
        return true;
    }
    return false;
}

const ScMatrix* ScDocument::GetDdeLinkResultMatrix( size_t nDdePos ) const
{
    const ScDdeLink* pDdeLink = lclGetDdeLink( GetLinkManager(), nDdePos );
    return pDdeLink ? pDdeLink->GetResult() : NULL;
}

bool ScDocument::CreateDdeLink( const OUString& rAppl, const OUString& rTopic, const OUString& rItem, sal_uInt8 nMode, ScMatrixRef pResults )
{
    /*  Create a DDE link without updating it (i.e. for Excel import), to prevent
        unwanted connections. First try to find existing link. Set result array
        on existing and new links. */
    //! store DDE links additionally at document (for efficiency)?
    OSL_ENSURE( nMode != SC_DDE_IGNOREMODE, "ScDocument::CreateDdeLink - SC_DDE_IGNOREMODE not allowed here" );

    sfx2::LinkManager* pMgr = GetDocLinkManager().getLinkManager(bAutoCalc);
    if (!pMgr)
        return false;

    if (nMode != SC_DDE_IGNOREMODE)
    {
        ScDdeLink* pDdeLink = lclGetDdeLink(pMgr, rAppl, rTopic, rItem, nMode);
        if( !pDdeLink )
        {
            // create a new DDE link, but without TryUpdate
            pDdeLink = new ScDdeLink( this, rAppl, rTopic, rItem, nMode );
            pMgr->InsertDDELink(pDdeLink, rAppl, rTopic, rItem);
        }

        // insert link results
        if( pResults )
            pDdeLink->SetResult( pResults );

        return true;
    }
    return false;
}

bool ScDocument::SetDdeLinkResultMatrix( size_t nDdePos, ScMatrixRef pResults )
{
    if( ScDdeLink* pDdeLink = lclGetDdeLink( GetLinkManager(), nDdePos ) )
    {
        pDdeLink->SetResult( pResults );
        return true;
    }
    return false;
}

bool ScDocument::HasAreaLinks() const
{
    const sfx2::LinkManager* pMgr = GetDocLinkManager().getExistingLinkManager();
    if (!pMgr)
        return false;

    const ::sfx2::SvBaseLinks& rLinks = pMgr->GetLinks();
    sal_uInt16 nCount = rLinks.size();
    for (sal_uInt16 i=0; i<nCount; i++)
        if ((*rLinks[i])->ISA(ScAreaLink))
            return true;

    return false;
}

void ScDocument::UpdateAreaLinks()
{
    sfx2::LinkManager* pMgr = GetDocLinkManager().getLinkManager(false);
    if (!pMgr)
        return;

    const ::sfx2::SvBaseLinks& rLinks = pMgr->GetLinks();
    for (sal_uInt16 i=0; i<rLinks.size(); i++)
    {
        ::sfx2::SvBaseLink* pBase = *rLinks[i];
        if (pBase->ISA(ScAreaLink))
            pBase->Update();
    }
}

void ScDocument::DeleteAreaLinksOnTab( SCTAB nTab )
{
    sfx2::LinkManager* pMgr = GetDocLinkManager().getLinkManager(false);
    if (!pMgr)
        return;

    const ::sfx2::SvBaseLinks& rLinks = pMgr->GetLinks();
    sal_uInt16 nPos = 0;
    while ( nPos < rLinks.size() )
    {
        const ::sfx2::SvBaseLink* pBase = *rLinks[nPos];
        if ( pBase->ISA(ScAreaLink) &&
             static_cast<const ScAreaLink*>(pBase)->GetDestArea().aStart.Tab() == nTab )
            pMgr->Remove(nPos);
        else
            ++nPos;
    }
}

void ScDocument::UpdateRefAreaLinks( UpdateRefMode eUpdateRefMode,
                             const ScRange& rRange, SCsCOL nDx, SCsROW nDy, SCsTAB nDz )
{
    sfx2::LinkManager* pMgr = GetDocLinkManager().getLinkManager(false);
    if (!pMgr)
        return;

    bool bAnyUpdate = false;

    const ::sfx2::SvBaseLinks& rLinks = pMgr->GetLinks();
    sal_uInt16 nCount = rLinks.size();
    for (sal_uInt16 i=0; i<nCount; i++)
    {
        ::sfx2::SvBaseLink* pBase = *rLinks[i];
        if (pBase->ISA(ScAreaLink))
        {
            ScAreaLink* pLink = (ScAreaLink*) pBase;
            ScRange aOutRange = pLink->GetDestArea();

            SCCOL nCol1 = aOutRange.aStart.Col();
            SCROW nRow1 = aOutRange.aStart.Row();
            SCTAB nTab1 = aOutRange.aStart.Tab();
            SCCOL nCol2 = aOutRange.aEnd.Col();
            SCROW nRow2 = aOutRange.aEnd.Row();
            SCTAB nTab2 = aOutRange.aEnd.Tab();

            ScRefUpdateRes eRes =
                ScRefUpdate::Update( this, eUpdateRefMode,
                    rRange.aStart.Col(), rRange.aStart.Row(), rRange.aStart.Tab(),
                    rRange.aEnd.Col(), rRange.aEnd.Row(), rRange.aEnd.Tab(), nDx, nDy, nDz,
                    nCol1, nRow1, nTab1, nCol2, nRow2, nTab2 );
            if ( eRes != UR_NOTHING )
            {
                pLink->SetDestArea( ScRange( nCol1, nRow1, nTab1, nCol2, nRow2, nTab2 ) );
                bAnyUpdate = true;
            }
        }
    }

    if ( bAnyUpdate )
    {
        // #i52120# Look for duplicates (after updating all positions).
        // If several links start at the same cell, the one with the lower index is removed
        // (file format specifies only one link definition for a cell).

        sal_uInt16 nFirstIndex = 0;
        while ( nFirstIndex < nCount )
        {
            bool bFound = false;
            ::sfx2::SvBaseLink* pFirst = *rLinks[nFirstIndex];
            if ( pFirst->ISA(ScAreaLink) )
            {
                ScAddress aFirstPos = static_cast<ScAreaLink*>(pFirst)->GetDestArea().aStart;
                for ( sal_uInt16 nSecondIndex = nFirstIndex + 1; nSecondIndex < nCount && !bFound; ++nSecondIndex )
                {
                    ::sfx2::SvBaseLink* pSecond = *rLinks[nSecondIndex];
                    if ( pSecond->ISA(ScAreaLink) &&
                         static_cast<ScAreaLink*>(pSecond)->GetDestArea().aStart == aFirstPos )
                    {
                        // remove the first link, exit the inner loop, don't increment nFirstIndex
                        pMgr->Remove(pFirst);
                        nCount = rLinks.size();
                        bFound = true;
                    }
                }
            }
            if (!bFound)
                ++nFirstIndex;
        }
    }
}

// TimerDelays etc.
void ScDocument::KeyInput( const KeyEvent& )
{
    if ( pChartListenerCollection->hasListeners() )
        pChartListenerCollection->StartTimer();
    if( apTemporaryChartLock.get() )
        apTemporaryChartLock->StartOrContinueLocking();
}

bool ScDocument::CheckMacroWarn()
{
    //  The check for macro configuration, macro warning and disabling is now handled
    //  in SfxObjectShell::AdjustMacroMode, called by SfxObjectShell::CallBasic.

    return true;
}

SfxBindings* ScDocument::GetViewBindings()
{
    //  used to invalidate slots after changes to this document

    if ( !pShell )
        return NULL;        // no ObjShell -> no view

    //  first check current view
    SfxViewFrame* pViewFrame = SfxViewFrame::Current();
    if ( pViewFrame && pViewFrame->GetObjectShell() != pShell )     // wrong document?
        pViewFrame = NULL;

    //  otherwise use first view for this doc
    if ( !pViewFrame )
        pViewFrame = SfxViewFrame::GetFirst( pShell );

    if (pViewFrame)
        return &pViewFrame->GetBindings();
    else
        return NULL;
}

void ScDocument::TransliterateText( const ScMarkData& rMultiMark, sal_Int32 nType )
{
    OSL_ENSURE( rMultiMark.IsMultiMarked(), "TransliterateText: no selection" );

    utl::TransliterationWrapper aTranslitarationWrapper( comphelper::getProcessComponentContext(), nType );
    bool bConsiderLanguage = aTranslitarationWrapper.needLanguageForTheMode();
    sal_uInt16 nLanguage = LANGUAGE_SYSTEM;

    boost::scoped_ptr<ScEditEngineDefaulter> pEngine;        // not using pEditEngine member because of defaults

    SCTAB nCount = GetTableCount();
    ScMarkData::const_iterator itr = rMultiMark.begin(), itrEnd = rMultiMark.end();
    for (; itr != itrEnd && *itr < nCount; ++itr)
        if ( maTabs[*itr] )
        {
            SCTAB nTab = *itr;
            SCCOL nCol = 0;
            SCROW nRow = 0;

            bool bFound = rMultiMark.IsCellMarked( nCol, nRow );
            if (!bFound)
                bFound = GetNextMarkedCell( nCol, nRow, nTab, rMultiMark );

            while (bFound)
            {
                ScRefCellValue aCell;
                aCell.assign(*this, ScAddress(nCol, nRow, nTab));

                // fdo#32786 TITLE_CASE/SENTENCE_CASE need the extra handling in EditEngine (loop over words/sentences).
                // Still use TransliterationWrapper directly for text cells with other transliteration types,
                // for performance reasons.
                if (aCell.meType == CELLTYPE_EDIT ||
                    (aCell.meType == CELLTYPE_STRING &&
                     ( nType == i18n::TransliterationModulesExtra::SENTENCE_CASE || nType == i18n::TransliterationModulesExtra::TITLE_CASE)))
                {
                    if (!pEngine)
                        pEngine.reset(new ScFieldEditEngine(this, GetEnginePool(), GetEditPool()));

                    // defaults from cell attributes must be set so right language is used
                    const ScPatternAttr* pPattern = GetPattern( nCol, nRow, nTab );
                    SfxItemSet* pDefaults = new SfxItemSet( pEngine->GetEmptyItemSet() );
                    if ( ScStyleSheet* pPreviewStyle = GetPreviewCellStyle( nCol, nRow, nTab ) )
                    {
                        boost::scoped_ptr<ScPatternAttr> pPreviewPattern(new ScPatternAttr( *pPattern ));
                        pPreviewPattern->SetStyleSheet(pPreviewStyle);
                        pPreviewPattern->FillEditItemSet( pDefaults );
                    }
                    else
                    {
                        SfxItemSet* pFontSet = GetPreviewFont( nCol, nRow, nTab );
                        pPattern->FillEditItemSet( pDefaults, pFontSet );
                    }
                    pEngine->SetDefaults( pDefaults,  true );
                    if (aCell.meType == CELLTYPE_STRING)
                        pEngine->SetText(aCell.mpString->getString());
                    else if (aCell.mpEditText)
                        pEngine->SetText(*aCell.mpEditText);

                    pEngine->ClearModifyFlag();

                    sal_Int32 nLastPar = pEngine->GetParagraphCount();
                    if (nLastPar)
                        --nLastPar;
                    sal_Int32 nTxtLen = pEngine->GetTextLen(nLastPar);
                    ESelection aSelAll( 0, 0, nLastPar, nTxtLen );

                    pEngine->TransliterateText( aSelAll, nType );

                    if ( pEngine->IsModified() )
                    {
                        ScEditAttrTester aTester( pEngine.get() );
                        if ( aTester.NeedsObject() )
                        {
                            // remove defaults (paragraph attributes) before creating text object
                            SfxItemSet* pEmpty = new SfxItemSet( pEngine->GetEmptyItemSet() );
                            pEngine->SetDefaults( pEmpty, true );

                            // The cell will take ownership of the text object instance.
                            SetEditText(ScAddress(nCol,nRow,nTab), pEngine->CreateTextObject());
                        }
                        else
                        {
                            ScSetStringParam aParam;
                            aParam.setTextInput();
                            SetString(ScAddress(nCol,nRow,nTab), pEngine->GetText(), &aParam);
                        }
                    }
                }

                else if (aCell.meType == CELLTYPE_STRING)
                {
                    OUString aOldStr = aCell.mpString->getString();
                    sal_Int32 nOldLen = aOldStr.getLength();

                    if ( bConsiderLanguage )
                    {
                        sal_uInt8 nScript = GetStringScriptType( aOldStr );        //! cell script type?
                        sal_uInt16 nWhich = ( nScript == SCRIPTTYPE_ASIAN ) ? ATTR_CJK_FONT_LANGUAGE :
                                        ( ( nScript == SCRIPTTYPE_COMPLEX ) ? ATTR_CTL_FONT_LANGUAGE :
                                                                                ATTR_FONT_LANGUAGE );
                        nLanguage = ((const SvxLanguageItem*)GetAttr( nCol, nRow, nTab, nWhich ))->GetValue();
                    }

                    uno::Sequence<sal_Int32> aOffsets;
                    OUString aNewStr = aTranslitarationWrapper.transliterate( aOldStr, nLanguage, 0, nOldLen, &aOffsets );

                    if ( aNewStr != aOldStr )
                    {
                        ScSetStringParam aParam;
                        aParam.setTextInput();
                        SetString(ScAddress(nCol,nRow,nTab), aNewStr, &aParam);
                    }
                }
                bFound = GetNextMarkedCell( nCol, nRow, nTab, rMultiMark );
            }
        }
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
