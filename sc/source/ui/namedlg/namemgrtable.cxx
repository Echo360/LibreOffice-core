/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

//ScRangeManagerTable
#include "global.hxx"
#include "reffact.hxx"
#include "document.hxx"
#include "docfunc.hxx"
#include "scresid.hxx"
#include "globstr.hrc"
#include "namedlg.hxx"
#include "viewdata.hxx"
#include "globalnames.hxx"

#include <sfx2/app.hxx>

#define ITEMID_NAME 1
#define ITEMID_RANGE 2
#define ITEMID_SCOPE 3

#define MINSIZE 80

static OUString createEntryString(const ScRangeNameLine& rLine)
{
    OUString aRet = rLine.aName + "\t" + rLine.aExpression + "\t" + rLine.aScope;
    return aRet;
}

ScRangeManagerTable::InitListener::~InitListener() {}

ScRangeManagerTable::ScRangeManagerTable( SvSimpleTableContainer& rParent, boost::ptr_map<OUString, ScRangeName>& rRangeMap, const ScAddress& rPos ):
    SvSimpleTable( rParent, WB_SORT | WB_HSCROLL | WB_CLIPCHILDREN | WB_TABSTOP ),
    maGlobalString( ScGlobal::GetRscString(STR_GLOBAL_SCOPE)),
    mrRangeMap( rRangeMap ),
    maPos( rPos ),
    mpInitListener(NULL)
{
    static long aStaticTabs[] = {3, 0, 0, 0 };
    SetTabs( &aStaticTabs[0], MAP_PIXEL );

    OUString aNameStr(ScGlobal::GetRscString(STR_HEADER_NAME));
    OUString aRangeStr(ScGlobal::GetRscString(STR_HEADER_RANGE));
    OUString aScopeStr(ScGlobal::GetRscString(STR_HEADER_SCOPE));

    HeaderBar& rHeaderBar = GetTheHeaderBar();
    rHeaderBar.InsertItem( ITEMID_NAME, aNameStr, 0, HIB_LEFT| HIB_VCENTER );
    rHeaderBar.InsertItem( ITEMID_RANGE, aRangeStr, 0, HIB_LEFT| HIB_VCENTER );
    rHeaderBar.InsertItem( ITEMID_SCOPE, aScopeStr, 0, HIB_LEFT| HIB_VCENTER );
    rHeaderBar.SetEndDragHdl( LINK( this, ScRangeManagerTable, HeaderEndDragHdl ) );

    setColWidths();
    UpdateViewSize();
    Init();
    ShowTable();
    SetSelectionMode(MULTIPLE_SELECTION);
    SetScrolledHdl( LINK( this, ScRangeManagerTable, ScrollHdl ) );
    void* pNull = NULL;
    HeaderEndDragHdl(pNull);
}

void ScRangeManagerTable::Resize()
{
    SvSimpleTable::Resize();
    if (isInitialLayout(this))
        setColWidths();
}

void ScRangeManagerTable::StateChanged( StateChangedType nStateChange )
{
    SvSimpleTable::StateChanged(nStateChange);

    if (nStateChange == STATE_CHANGE_INITSHOW)
    {
        if (GetEntryCount())
        {
            SetCurEntry(GetEntryOnPos(0));
            CheckForFormulaString();
        }

        if (mpInitListener)
            mpInitListener->tableInitialized();
    }
}

void ScRangeManagerTable::setColWidths()
{
    HeaderBar &rHeaderBar = GetTheHeaderBar();
    if (rHeaderBar.GetItemCount() < 3)
        return;
    long nTabSize = GetSizePixel().Width() / 3;
    rHeaderBar.SetItemSize( ITEMID_NAME, nTabSize);
    rHeaderBar.SetItemSize( ITEMID_RANGE, nTabSize);
    rHeaderBar.SetItemSize( ITEMID_SCOPE, nTabSize);
    static long aStaticTabs[] = {3, 0, nTabSize, 2*nTabSize };
    SetTabs( &aStaticTabs[0], MAP_PIXEL );
    void* pNull = NULL;
    HeaderEndDragHdl(pNull);
}

ScRangeManagerTable::~ScRangeManagerTable()
{
    Clear();
}

void ScRangeManagerTable::setInitListener( InitListener* pListener )
{
    mpInitListener = pListener;
}

void ScRangeManagerTable::addEntry(const ScRangeNameLine& rLine, bool bSetCurEntry)
{
    SvTreeListEntry* pEntry = InsertEntryToColumn( createEntryString(rLine), TREELIST_APPEND, 0xffff);
    if (bSetCurEntry)
        SetCurEntry(pEntry);
}

void ScRangeManagerTable::GetCurrentLine(ScRangeNameLine& rLine)
{
    SvTreeListEntry* pCurrentEntry = GetCurEntry();
    GetLine(rLine, pCurrentEntry);
}

void ScRangeManagerTable::GetLine(ScRangeNameLine& rLine, SvTreeListEntry* pEntry)
{
    rLine.aName = GetEntryText( pEntry, 0);
    rLine.aExpression = GetEntryText(pEntry, 1);
    rLine.aScope = GetEntryText(pEntry, 2);
}

void ScRangeManagerTable::Init()
{
    SetUpdateMode(false);
    Clear();
    for (boost::ptr_map<OUString, ScRangeName>::const_iterator itr = mrRangeMap.begin();
            itr != mrRangeMap.end(); ++itr)
    {
        const ScRangeName* pLocalRangeName = itr->second;
        ScRangeNameLine aLine;
        if ( itr->first == STR_GLOBAL_RANGE_NAME )
            aLine.aScope = maGlobalString;
        else
            aLine.aScope = itr->first;
        for (ScRangeName::const_iterator it = pLocalRangeName->begin();
                it != pLocalRangeName->end(); ++it)
        {
            if (!it->second->HasType(RT_DATABASE))
            {
                aLine.aName = it->second->GetName();
                addEntry(aLine, false);
            }
        }
    }
    SetUpdateMode(true);
}

const ScRangeData* ScRangeManagerTable::findRangeData(const ScRangeNameLine& rLine)
{
    const ScRangeName* pRangeName;
    if (rLine.aScope == maGlobalString)
        pRangeName = mrRangeMap.find(OUString(STR_GLOBAL_RANGE_NAME))->second;
    else
        pRangeName = mrRangeMap.find(rLine.aScope)->second;

    return pRangeName->findByUpperName(ScGlobal::pCharClass->uppercase(rLine.aName));
}

void ScRangeManagerTable::CheckForFormulaString()
{
    for (SvTreeListEntry* pEntry = GetFirstEntryInView(); pEntry ; pEntry = GetNextEntryInView(pEntry))
    {
        std::map<SvTreeListEntry*, bool>::const_iterator itr = maCalculatedFormulaEntries.find(pEntry);
        if (itr == maCalculatedFormulaEntries.end() || itr->second == false)
        {
            ScRangeNameLine aLine;
            GetLine( aLine, pEntry);
            const ScRangeData* pData = findRangeData( aLine );
            OUString aFormulaString;
            pData->GetSymbol(aFormulaString, maPos);
            SetEntryText(aFormulaString, pEntry, 1);
            maCalculatedFormulaEntries.insert( std::pair<SvTreeListEntry*, bool>(pEntry, true) );
        }
    }
}

void ScRangeManagerTable::DeleteSelectedEntries()
{
    if (GetSelectionCount())
        RemoveSelection();
}

bool ScRangeManagerTable::IsMultiSelection()
{
    return GetSelectionCount() > 1;
}

std::vector<ScRangeNameLine> ScRangeManagerTable::GetSelectedEntries()
{
    std::vector<ScRangeNameLine> aSelectedEntries;
    if (GetSelectionCount())
    {
        for (SvTreeListEntry* pEntry = FirstSelected(); pEntry != LastSelected(); pEntry = NextSelected(pEntry))
        {
            ScRangeNameLine aLine;
            GetLine( aLine, pEntry );
            aSelectedEntries.push_back(aLine);
        }
        SvTreeListEntry* pEntry = LastSelected();
        ScRangeNameLine aLine;
        GetLine( aLine, pEntry );
        aSelectedEntries.push_back(aLine);
    }
    return aSelectedEntries;
}

void ScRangeManagerTable::SetEntry(const ScRangeNameLine& rLine)
{
    for (SvTreeListEntry* pEntry = First(); pEntry; pEntry = Next(pEntry))
    {
        if (rLine.aName == GetEntryText(pEntry, 0)
                && rLine.aScope == GetEntryText(pEntry, 2))
        {
            SetCurEntry(pEntry);
        }
    }
}

namespace {

//ensure that the minimum column size is respected
void CalculateItemSize(const long& rTableSize, long& rItemNameSize, long& rItemRangeSize)
{
    long aItemScopeSize = rTableSize - rItemNameSize - rItemRangeSize;

    if (rItemNameSize >= MINSIZE && rItemRangeSize >= MINSIZE && aItemScopeSize >= MINSIZE)
        return;

    if (rItemNameSize < MINSIZE)
    {
        long aDiffSize = MINSIZE - rItemNameSize;
        if (rItemRangeSize > aItemScopeSize)
            rItemRangeSize -= aDiffSize;
        else
            aItemScopeSize -= aDiffSize;
        rItemNameSize = MINSIZE;
    }

    if (rItemRangeSize < MINSIZE)
    {
        long aDiffSize = MINSIZE - rItemRangeSize;
        if (rItemNameSize > aItemScopeSize)
            rItemNameSize -= aDiffSize;
        else
            aItemScopeSize -= aDiffSize;
        rItemRangeSize = MINSIZE;
    }

    if (aItemScopeSize < MINSIZE)
    {
        long aDiffSize = MINSIZE - aItemScopeSize;
        if (rItemNameSize > rItemRangeSize)
            rItemNameSize -= aDiffSize;
        else
            rItemRangeSize -= aDiffSize;
    }
}

}

IMPL_LINK_NOARG(ScRangeManagerTable, HeaderEndDragHdl)
{
    HeaderBar& rHeaderBar = GetTheHeaderBar();

    long nTableSize = rHeaderBar.GetSizePixel().Width();
    long nItemNameSize = rHeaderBar.GetItemSize(ITEMID_NAME);
    long nItemRangeSize = rHeaderBar.GetItemSize(ITEMID_RANGE);

    //calculate column size based on user input and minimum size
    CalculateItemSize(nTableSize, nItemNameSize, nItemRangeSize);
    long nItemScopeSize = nTableSize - nItemNameSize - nItemRangeSize;

    Size aSz(nItemNameSize, 0);
    rHeaderBar.SetItemSize(ITEMID_NAME, nItemNameSize);
    rHeaderBar.SetItemSize(ITEMID_RANGE, nItemRangeSize);
    rHeaderBar.SetItemSize(ITEMID_SCOPE, nItemScopeSize);

    SetTab(0, 0, MAP_APPFONT );
    SetTab(1, PixelToLogic( aSz, MapMode(MAP_APPFONT) ).Width(), MAP_APPFONT );
    aSz.Width() += nItemRangeSize;
    SetTab(2, PixelToLogic( aSz, MapMode(MAP_APPFONT) ).Width(), MAP_APPFONT );

    return 0;
}

IMPL_LINK_NOARG(ScRangeManagerTable, ScrollHdl)
{
    CheckForFormulaString();
    return 0;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
