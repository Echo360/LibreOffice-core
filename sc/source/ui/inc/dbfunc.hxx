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

#ifndef INCLUDED_SC_SOURCE_UI_INC_DBFUNC_HXX
#define INCLUDED_SC_SOURCE_UI_INC_DBFUNC_HXX

#include "viewfunc.hxx"
#include "dptypes.hxx"

namespace com { namespace sun { namespace star { namespace sheet {
    struct DataPilotFieldFilter;
}}}}

struct ScSortParam;
struct ScQueryParam;
class ScDBData;
class ScDBCollection;
class ScDPObject;
class ScDPSaveData;
struct ScDPNumGroupInfo;
struct ScSubTotalParam;

class ScDBFunc : public ScViewFunc
{
private:
    void GetSelectedMemberList(ScDPUniqueStringSet& rEntries, long& rDimension);

public:
                    ScDBFunc( Window* pParent, ScDocShell& rDocSh, ScTabViewShell* pViewShell );
    virtual         ~ScDBFunc();

                    // only UISort repeat the partial results if necessary

    void            UISort( const ScSortParam& rSortParam,
                          bool bRecord = true );

    void            Sort( const ScSortParam& rSortParam,
                          bool bRecord = true, bool bPaint = true );
    SC_DLLPUBLIC void           Query( const ScQueryParam& rQueryParam,
                           const ScRange* pAdvSource, bool bRecord );
    void            DoSubTotals( const ScSubTotalParam& rParam, bool bRecord = true,
                            const ScSortParam* pForceNewSort = NULL );

    void            ToggleAutoFilter();
    void            HideAutoFilter();

    void            RepeatDB( bool bRecord = true );

    bool            ImportData( const ScImportParam& rParam, bool bRecord = true );

    void GotoDBArea( const OUString& rDBName );

                    // DB range from Cursor
    ScDBData*       GetDBData( bool bMarkArea = true, ScGetDBMode eMode = SC_DB_MAKE, ScGetDBSelection eSel = SC_DBSEL_KEEP);
    ScDBData*       GetAnonymousDBData();

    void            Consolidate( const ScConsolidateParam& rParam, bool bRecord = true );

    bool MakePivotTable(
        const ScDPSaveData& rData, const ScRange& rDest, bool bNewTable,
        const ScDPObject& rSource, bool bApi = false );

    void            DeletePivotTable();
    void            RecalcPivotTable();
    bool HasSelectionForDateGroup( ScDPNumGroupInfo& rOldInfo, sal_Int32& rParts );
    bool HasSelectionForNumGroup( ScDPNumGroupInfo& rOldInfo );
    void            GroupDataPilot();
    void            DateGroupDataPilot( const ScDPNumGroupInfo& rInfo, sal_Int32 nParts );
    void            NumGroupDataPilot( const ScDPNumGroupInfo& rInfo );
    void            UngroupDataPilot();
    void DataPilotInput( const ScAddress& rPos, const OUString& rString );

    bool            DataPilotSort( const ScAddress& rPos, bool bAscending, sal_uInt16* pUserListId = NULL );
    bool            DataPilotMove( const ScRange& rSource, const ScAddress& rDest );

    bool HasSelectionForDrillDown( sal_uInt16& rOrientation );
    void SetDataPilotDetails(bool bShow, const OUString* pNewDimensionName = NULL);

    void            ShowDataPilotSourceData( ScDPObject& rDPObj,
                        const ::com::sun::star::uno::Sequence< ::com::sun::star::sheet::DataPilotFieldFilter >& rFilters );

    void            MakeOutline( bool bColumns, bool bRecord = true );
    void            RemoveOutline( bool bColumns, bool bRecord = true );
    void            RemoveAllOutlines( bool bRecord = true );
    void            TestRemoveOutline( bool& rCol, bool& rRow );

    void            AutoOutline( bool bRecord = true );

    void            SelectLevel( bool bColumns, sal_uInt16 nLevel,
                                    bool bRecord = true, bool bPaint = true );
    void            ShowOutline( bool bColumns, sal_uInt16 nLevel, sal_uInt16 nEntry,
                                    bool bRecord = true, bool bPaint = true );
    void            HideOutline( bool bColumns, sal_uInt16 nLevel, sal_uInt16 nEntry,
                                    bool bRecord = true, bool bPaint = true );

    void            ShowMarkedOutlines( bool bRecord = true );
    void            HideMarkedOutlines( bool bRecord = true );
    bool            OutlinePossible(bool bHide);

    void            UpdateCharts(bool bAllCharts = false);      // Default: am Cursor

    static sal_uInt16   DoUpdateCharts( const ScAddress& rPos, ScDocument* pDoc, bool bAllCharts );
};

#endif

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
