/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#undef SC_DLLIMPLEMENTATION

#include "tpdefaults.hxx"
#include "sc.hrc"
#include "scresid.hxx"
#include "scmod.hxx"
#include "defaultsoptions.hxx"
#include "document.hxx"

ScTpDefaultsOptions::ScTpDefaultsOptions(Window *pParent, const SfxItemSet &rCoreSet) :
    SfxTabPage(pParent, "OptDefaultPage", "modules/scalc/ui/optdefaultpage.ui", &rCoreSet)

{
    get( m_pEdNSheets, "sheetsnumber");
    get( m_pEdSheetPrefix, "sheetprefix");

    m_pEdNSheets->SetModifyHdl( LINK(this, ScTpDefaultsOptions, NumModifiedHdl) );
    m_pEdSheetPrefix->SetModifyHdl( LINK(this, ScTpDefaultsOptions, PrefixModifiedHdl) );
    m_pEdSheetPrefix->SetGetFocusHdl( LINK(this, ScTpDefaultsOptions, PrefixEditOnFocusHdl) );
}

ScTpDefaultsOptions::~ScTpDefaultsOptions()
{
}

SfxTabPage* ScTpDefaultsOptions::Create(Window *pParent, const SfxItemSet *rCoreAttrs)
{
    return new ScTpDefaultsOptions(pParent, *rCoreAttrs);
}

bool ScTpDefaultsOptions::FillItemSet(SfxItemSet *rCoreSet)
{
    bool bRet = false;
    ScDefaultsOptions aOpt;

    SCTAB nTabCount = static_cast<SCTAB>(m_pEdNSheets->GetValue());
    OUString aSheetPrefix = m_pEdSheetPrefix->GetText();

    if ( m_pEdNSheets->IsValueChangedFromSaved()
         || m_pEdSheetPrefix->GetSavedValue() != aSheetPrefix )
    {
        aOpt.SetInitTabCount( nTabCount );
        aOpt.SetInitTabPrefix( aSheetPrefix );

        rCoreSet->Put( ScTpDefaultsItem( SID_SCDEFAULTSOPTIONS, aOpt ) );
        bRet = true;
    }
    return bRet;
}

void ScTpDefaultsOptions::Reset(const SfxItemSet* rCoreSet)
{
    ScDefaultsOptions aOpt;
    const SfxPoolItem* pItem = NULL;

    if(SFX_ITEM_SET == rCoreSet->GetItemState(SID_SCDEFAULTSOPTIONS, false , &pItem))
        aOpt = ((const ScTpDefaultsItem*)pItem)->GetDefaultsOptions();

    m_pEdNSheets->SetValue( static_cast<sal_uInt16>( aOpt.GetInitTabCount()) );
    m_pEdSheetPrefix->SetText( aOpt.GetInitTabPrefix() );
    m_pEdNSheets->SaveValue();
    m_pEdSheetPrefix->SaveValue();
}

int ScTpDefaultsOptions::DeactivatePage(SfxItemSet* /*pSet*/)
{
    return KEEP_PAGE;
}

void ScTpDefaultsOptions::CheckNumSheets()
{
    sal_Int64 nVal = m_pEdNSheets->GetValue();
    if (nVal > MAXINITTAB)
        m_pEdNSheets->SetValue(MAXINITTAB);
    if (nVal < MININITTAB)
        m_pEdNSheets->SetValue(MININITTAB);
}

void ScTpDefaultsOptions::CheckPrefix(Edit* pEdit)
{
    if (!pEdit)
        return;

    OUString aSheetPrefix = pEdit->GetText();

    if ( !aSheetPrefix.isEmpty() && !ScDocument::ValidTabName( aSheetPrefix ) )
    {
        // Revert to last good Prefix and also select it to
        // indicate something illegal was typed
        Selection aSel( 0,  maOldPrefixValue.getLength() );
        pEdit->SetText( maOldPrefixValue, aSel );
    }
    else
    {
        OnFocusPrefixInput(pEdit);
    }
}

void ScTpDefaultsOptions::OnFocusPrefixInput(Edit* pEdit)
{
    if (!pEdit)
        return;

    // Store Prefix in case we need to revert
    maOldPrefixValue = pEdit->GetText();
}

IMPL_LINK_NOARG(ScTpDefaultsOptions, NumModifiedHdl)
{
    CheckNumSheets();
    return 0;
}

IMPL_LINK( ScTpDefaultsOptions, PrefixModifiedHdl, Edit*, pEdit )
{
    CheckPrefix(pEdit);
    return 0;
}

IMPL_LINK( ScTpDefaultsOptions, PrefixEditOnFocusHdl, Edit*, pEdit )
{
    OnFocusPrefixInput(pEdit);
    return 0;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
