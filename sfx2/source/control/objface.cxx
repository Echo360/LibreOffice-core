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

#include <assert.h>
#include <stdlib.h>

#include <sal/log.hxx>

#include <tools/rcid.h>
#include <tools/stream.hxx>
#include "tools/resmgr.hxx"

#include <sfx2/module.hxx>
#include <sfx2/objface.hxx>
#include <sfx2/msg.hxx>
#include <sfx2/app.hxx>
#include <sfx2/msgpool.hxx>
#include <sfx2/sfxresid.hxx>
#include <sfx2/objsh.hxx>
#include <rtl/strbuf.hxx>

extern "C" {

static int SAL_CALL
SfxCompareSlots_qsort( const void* pSmaller, const void* pBigger )
{
    return ( (int) ((SfxSlot*)pSmaller)->GetSlotId() ) -
           ( (int) ((SfxSlot*)pBigger)->GetSlotId() );
}

static int SAL_CALL
SfxCompareSlots_bsearch( const void* pSmaller, const void* pBigger )
{
    return ( (int) *((sal_uInt16*)pSmaller) ) -
           ( (int) ((SfxSlot*)pBigger)->GetSlotId() );
}

}

struct SfxObjectUI_Impl
{
    sal_uInt16  nPos;
    ResId       aResId;
    bool        bVisible;
    bool        bContext;
    OUString*   pName;
    sal_uInt32  nFeature;

    SfxObjectUI_Impl(sal_uInt16 n, const ResId& rResId, bool bVis, sal_uInt32 nFeat) :
        nPos(n),
        aResId(rResId.GetId(), *rResId.GetResMgr()),
        bVisible(bVis),
        bContext(false),
        pName(0),
        nFeature(nFeat)
    {
        aResId.SetRT(rResId.GetRT());
    }

    ~SfxObjectUI_Impl()
    {
        delete pName;
    }
};

typedef std::vector<SfxObjectUI_Impl*> SfxObjectUIArr_Impl;

struct SfxInterface_Impl
{
    SfxObjectUIArr_Impl     aObjectBars;    // registered ObjectBars
    SfxObjectUIArr_Impl     aChildWindows;  // registered ChildWindows
    ResId                   aPopupRes;      // registered PopupMenu
    ResId                   aStatBarRes;    // registered StatusBar
    SfxModule*              pModule;
    bool                    bRegistered;

    SfxInterface_Impl() :
        aPopupRes(0,*SfxApplication::GetOrCreate()->GetSfxResManager()),
        aStatBarRes(0,*SfxApplication::GetOrCreate()->GetSfxResManager())
    , pModule(NULL)
    , bRegistered(false)
    {
    }

    ~SfxInterface_Impl()
    {
        for (SfxObjectUIArr_Impl::const_iterator it = aObjectBars.begin(); it != aObjectBars.end(); ++it)
            delete *it;

        for (SfxObjectUIArr_Impl::const_iterator it = aChildWindows.begin(); it != aChildWindows.end(); ++it)
            delete *it;
    }
};

static SfxObjectUI_Impl* CreateObjectBarUI_Impl( sal_uInt16 nPos, const ResId& rResId, sal_uInt32 nFeature, const OUString *pStr );




// constuctor, registeres a new unit

SfxInterface::SfxInterface( const char *pClassName,
                            const ResId& rNameResId,
                            SfxInterfaceId nId,
                            const SfxInterface* pParent,
                            SfxSlot &rSlotMap, sal_uInt16 nSlotCount ):
    pName(pClassName),
    pGenoType(pParent),
    nClassId(nId),
    aNameResId(rNameResId.GetId(),*rNameResId.GetResMgr()),
    pImpData(0)
{
    pImpData = new SfxInterface_Impl;
    SetSlotMap( rSlotMap, nSlotCount );
}

void SfxInterface::Register( SfxModule* pMod )
{
    pImpData->bRegistered = true;
    pImpData->pModule = pMod;
    if ( pMod )
        pMod->GetSlotPool()->RegisterInterface(*this);
    else
        SfxGetpApp()->GetAppSlotPool_Impl().RegisterInterface(*this);
}

void SfxInterface::SetSlotMap( SfxSlot& rSlotMap, sal_uInt16 nSlotCount )
{
    pSlots = &rSlotMap;
    nCount = nSlotCount;
    SfxSlot* pIter = pSlots;
    if ( 1 == nCount && !pIter->pNextSlot )
        pIter->pNextSlot = pIter;

    if ( !pIter->pNextSlot )
    {
        // sort the SfxSlots by id
        qsort( pSlots, nCount, sizeof(SfxSlot), SfxCompareSlots_qsort );

        // link masters and slaves
        sal_uInt16 nIter = 1;
        for ( pIter = pSlots; nIter <= nCount; ++pIter, ++nIter )
        {

            assert( nIter == nCount ||
                    pIter->GetSlotId() != (pIter+1)->GetSlotId() );

            // every master refers to his first slave (ENUM),
            // all slaves refer to their master.
            // Slaves refer in a circle to the other slaves with the same master
            if ( pIter->GetKind() == SFX_KIND_ENUM )
            {
                pIter->pLinkedSlot = GetSlot( pIter->nMasterSlotId );
                assert( pIter->pLinkedSlot );
                if ( !pIter->pLinkedSlot->pLinkedSlot )
                    ( (SfxSlot*) pIter->pLinkedSlot)->pLinkedSlot = pIter;

                if ( 0 == pIter->GetNextSlot() )
                {
                    SfxSlot *pLastSlot = pIter;
                    for ( sal_uInt16 n = nIter; n < Count(); ++n )
                    {
                        SfxSlot *pCurSlot = (pSlots+n);
                        if ( pCurSlot->nMasterSlotId == pIter->nMasterSlotId )
                        {
                            pLastSlot->pNextSlot = pCurSlot;
                            pLastSlot = pCurSlot;
                        }
                    }
                    pLastSlot->pNextSlot = pIter;
                }
            }
            else if ( 0 == pIter->GetNextSlot() )
            {
                // Slots refering in circle to the next with the same
                // Status method.
                SfxSlot *pLastSlot = pIter;
                for ( sal_uInt16 n = nIter; n < Count(); ++n )
                {
                    SfxSlot *pCurSlot = (pSlots+n);
                    if ( pCurSlot->GetStateFnc() == pIter->GetStateFnc() )
                    {
                        pLastSlot->pNextSlot = pCurSlot;
                        pLastSlot = pCurSlot;
                    }
                }
                pLastSlot->pNextSlot = pIter;
            }
        }
    }
#ifdef DBG_UTIL
    else
    {
        sal_uInt16 nIter = 1;
        for ( SfxSlot *pNext = pIter+1; nIter < nCount; ++pNext, ++nIter )
        {

            if ( pNext->GetSlotId() <= pIter->GetSlotId() )
                SAL_WARN( "sfx.control", "Wrong order" );

            if ( pIter->GetKind() == SFX_KIND_ENUM )
            {
                const SfxSlot *pMasterSlot = GetSlot(pIter->nMasterSlotId);
                const SfxSlot *pFirstSlave = pMasterSlot->pLinkedSlot;
                const SfxSlot *pSlave = pFirstSlave;
                do
                {
                    if ( pSlave->pLinkedSlot != pMasterSlot )
                    {
                        OStringBuffer aStr("Wrong Master/Slave- link: ");
                        aStr.append(static_cast<sal_Int32>(
                            pMasterSlot->GetSlotId()));
                        aStr.append(" , ");
                        aStr.append(static_cast<sal_Int32>(
                            pSlave->GetSlotId()));
                        SAL_WARN("sfx.control", aStr.getStr());
                    }

                    if ( pSlave->nMasterSlotId != pMasterSlot->GetSlotId() )
                    {
                        OStringBuffer aStr("Wrong Master/Slave-Ids: ");
                        aStr.append(static_cast<sal_Int32>(
                            pMasterSlot->GetSlotId()));
                        aStr.append(" , ");
                        aStr.append(static_cast<sal_Int32>(
                            pSlave->GetSlotId()));
                        SAL_WARN("sfx.control", aStr.getStr());
                    }

                    pSlave = pSlave->pNextSlot;
                }
                while ( pSlave != pFirstSlave );
            }
            else
            {
                if ( pIter->pLinkedSlot )
                {
                    if ( pIter->pLinkedSlot->GetKind() != SFX_KIND_ENUM )
                    {
                        OStringBuffer aStr("Slave is no enum: ");
                        aStr.append(static_cast<sal_Int32>(pIter->GetSlotId()));
                        aStr.append(" , ");
                        aStr.append(static_cast<sal_Int32>(
                            pIter->pLinkedSlot->GetSlotId()));
                        SAL_WARN("sfx.control", aStr.getStr());
                    }
                }

                const SfxSlot *pCurSlot = pIter;
                do
                {
                    pCurSlot = pCurSlot->pNextSlot;
                    if ( pCurSlot->GetStateFnc() != pIter->GetStateFnc() )
                    {
                        OStringBuffer aStr("Linked Slots with different State Methods : ");
                        aStr.append(static_cast<sal_Int32>(
                            pCurSlot->GetSlotId()));
                        aStr.append(" , ");
                        aStr.append(static_cast<sal_Int32>(pIter->GetSlotId()));
                        SAL_WARN("sfx.control", aStr.getStr());
                    }
                }
                while ( pCurSlot != pIter );
            }

            pIter = pNext;
        }
    }
#endif
}



SfxInterface::~SfxInterface()
{
    SfxModule *pMod = pImpData->pModule;
    bool bRegistered = pImpData->bRegistered;
    delete pImpData;
    assert( bRegistered );
    if ( bRegistered )
    {
        if ( pMod )
            pMod->GetSlotPool()->ReleaseInterface(*this);
        else
            SfxGetpApp()->GetAppSlotPool_Impl().ReleaseInterface(*this);
    }
}



// searches for the specified func

const SfxSlot* SfxInterface::GetSlot( sal_uInt16 nFuncId ) const
{

    assert( pSlots );
    assert( nCount );

    // find the id using binary search
    void* p = bsearch( &nFuncId, pSlots, nCount, sizeof(SfxSlot),
                       SfxCompareSlots_bsearch );
    if ( !p && pGenoType )
        return pGenoType->GetSlot( nFuncId );

    return p ? (const SfxSlot*)p : 0;
}

const SfxSlot* SfxInterface::GetSlot( const OUString& rCommand ) const
{
    static const char UNO_COMMAND[] = ".uno:";

    OUString aCommand( rCommand );
    if ( aCommand.startsWith( UNO_COMMAND ) )
         aCommand = aCommand.copy( sizeof( UNO_COMMAND )-1 );

    for ( sal_uInt16 n=0; n<nCount; n++ )
    {
        if ( (pSlots+n)->pUnoName &&
             aCommand.compareToIgnoreAsciiCaseAscii( (pSlots+n)->GetUnoName() ) == 0 )
            return pSlots+n;
    }

    return pGenoType ? pGenoType->GetSlot( aCommand ) : NULL;
}



const SfxSlot* SfxInterface::GetRealSlot( const SfxSlot *pSlot ) const
{

    assert( pSlots );
    assert( nCount );

    if ( !ContainsSlot_Impl(pSlot) )
    {
        if(pGenoType)
            return pGenoType->GetRealSlot(pSlot);
        SAL_WARN( "sfx.control", "unknown Slot" );
        return 0;
    }

    return pSlot->pLinkedSlot;
}



const SfxSlot* SfxInterface::GetRealSlot( sal_uInt16 nSlotId ) const
{

    assert( pSlots );
    assert( nCount );

    const SfxSlot *pSlot = GetSlot(nSlotId);
    if ( !pSlot )
    {
        if(pGenoType)
            return pGenoType->GetRealSlot(nSlotId);
        SAL_WARN( "sfx.control", "unknown Slot" );
        return 0;
    }

    return pSlot->pLinkedSlot;
}



void SfxInterface::RegisterPopupMenu( const ResId& rResId )
{

    pImpData->aPopupRes = rResId;
}



void SfxInterface::RegisterObjectBar( sal_uInt16 nPos, const ResId& rResId,
        const OUString *pStr )
{
    RegisterObjectBar( nPos, rResId, 0UL, pStr );
}


void SfxInterface::RegisterObjectBar( sal_uInt16 nPos, const ResId& rResId, sal_uInt32 nFeature, const OUString *pStr )
{
    SfxObjectUI_Impl* pUI = CreateObjectBarUI_Impl( nPos, rResId, nFeature, pStr );
    if ( pUI )
        pImpData->aObjectBars.push_back(pUI);
}

SfxObjectUI_Impl* CreateObjectBarUI_Impl( sal_uInt16 nPos, const ResId& rResId, sal_uInt32 nFeature, const OUString *pStr )
{
    if ((nPos & SFX_VISIBILITY_MASK) == 0)
        nPos |= SFX_VISIBILITY_STANDARD;

    SfxObjectUI_Impl* pUI = new SfxObjectUI_Impl(nPos, rResId, true, nFeature);

    if (pStr == 0)
    {
        ResId aResId(rResId);
        aResId.SetRT(RSC_STRING);
        aResId.SetResMgr(rResId.GetResMgr());
        if( ! aResId.GetResMgr() )
            aResId.SetResMgr( SfxApplication::GetOrCreate()->GetOffResManager_Impl() );
        if ( !aResId.GetResMgr() || !aResId.GetResMgr()->IsAvailable(aResId) )
            pUI->pName = new OUString ("NoName");
        else
            pUI->pName = new OUString(aResId.toString());
    }
    else
        pUI->pName = new OUString(*pStr);

    return pUI;
}

const ResId& SfxInterface::GetObjectBarResId( sal_uInt16 nNo ) const
{
    bool bGenoType = (pGenoType != 0 && !pGenoType->HasName());
    if ( bGenoType )
    {
        // Are there toolbars in the super class?
        sal_uInt16 nBaseCount = pGenoType->GetObjectBarCount();
        if ( nNo < nBaseCount )
            // The Super class comes first
            return pGenoType->GetObjectBarResId( nNo );
        else
            nNo = nNo - nBaseCount;
    }

    assert( nNo<pImpData->aObjectBars.size() );

    return pImpData->aObjectBars[nNo]->aResId;
}




sal_uInt16 SfxInterface::GetObjectBarPos( sal_uInt16 nNo ) const
{
    bool bGenoType = (pGenoType != 0 && !pGenoType->HasName());
    if ( bGenoType )
    {
        // Are there toolbars in the super class?
        sal_uInt16 nBaseCount = pGenoType->GetObjectBarCount();
        if ( nNo < nBaseCount )
            // The Super class comes first
            return pGenoType->GetObjectBarPos( nNo );
        else
            nNo = nNo - nBaseCount;
    }

    assert( nNo<pImpData->aObjectBars.size() );

    return pImpData->aObjectBars[nNo]->nPos;
}




sal_uInt16 SfxInterface::GetObjectBarCount() const
{
    if (pGenoType && ! pGenoType->HasName())
        return pImpData->aObjectBars.size() + pGenoType->GetObjectBarCount();
    else
        return pImpData->aObjectBars.size();
}


void SfxInterface::RegisterChildWindow(sal_uInt16 nId, bool bContext, const OUString* pChildWinName)
{
    RegisterChildWindow( nId, bContext, 0UL, pChildWinName );
}

void SfxInterface::RegisterChildWindow(sal_uInt16 nId, bool bContext, sal_uInt32 nFeature, const OUString*)
{
    SfxObjectUI_Impl* pUI = new SfxObjectUI_Impl(0, ResId(nId, *SfxApplication::GetOrCreate()->GetOffResManager_Impl()), true, nFeature);
    pUI->bContext = bContext;
    pImpData->aChildWindows.push_back(pUI);
}

void SfxInterface::RegisterStatusBar(const ResId& rResId)
{
    pImpData->aStatBarRes = rResId;
}


sal_uInt32 SfxInterface::GetChildWindowId (sal_uInt16 nNo) const
{
    if ( pGenoType )
    {
        // Are there ChildWindows in der Superklasse?
        sal_uInt16 nBaseCount = pGenoType->GetChildWindowCount();
        if ( nNo < nBaseCount )
            // The Super class comes first
            return pGenoType->GetChildWindowId( nNo );
        else
            nNo = nNo - nBaseCount;
    }

    assert( nNo<pImpData->aChildWindows.size() );

    sal_uInt32 nRet = pImpData->aChildWindows[nNo]->aResId.GetId();
    if ( pImpData->aChildWindows[nNo]->bContext )
        nRet += sal_uInt32( nClassId ) << 16;
    return nRet;
}

sal_uInt32 SfxInterface::GetChildWindowFeature (sal_uInt16 nNo) const
{
    if ( pGenoType )
    {
        // Are there ChildWindows in der Superklasse?
        sal_uInt16 nBaseCount = pGenoType->GetChildWindowCount();
        if ( nNo < nBaseCount )
            // The Super class comes first
            return pGenoType->GetChildWindowFeature( nNo );
        else
            nNo = nNo - nBaseCount;
    }

    assert( nNo<pImpData->aChildWindows.size() );

    return pImpData->aChildWindows[nNo]->nFeature;
}




sal_uInt16 SfxInterface::GetChildWindowCount() const
{
    if (pGenoType)
        return pImpData->aChildWindows.size() + pGenoType->GetChildWindowCount();
    else
        return pImpData->aChildWindows.size();
}


const ResId& SfxInterface::GetPopupMenuResId() const
{
    return pImpData->aPopupRes;
}


const ResId& SfxInterface::GetStatusBarResId() const
{
    if (pImpData->aStatBarRes.GetId() == 0 && pGenoType)
        return pGenoType->GetStatusBarResId();
    else
        return pImpData->aStatBarRes;
}



const OUString* SfxInterface::GetObjectBarName ( sal_uInt16 nNo ) const
{
    bool bGenoType = (pGenoType != 0 && !pGenoType->HasName());
    if ( bGenoType )
    {
        // Are there toolbars in the super class?
        sal_uInt16 nBaseCount = pGenoType->GetObjectBarCount();
        if ( nNo < nBaseCount )
            // The Super class comes first
            return pGenoType->GetObjectBarName( nNo );
        else
            nNo = nNo - nBaseCount;
    }

    assert( nNo<pImpData->aObjectBars.size() );

    return pImpData->aObjectBars[nNo]->pName;
}

sal_uInt32 SfxInterface::GetObjectBarFeature ( sal_uInt16 nNo ) const
{
    bool bGenoType = (pGenoType != 0 && !pGenoType->HasName());
    if ( bGenoType )
    {
        // Are there toolbars in the super class?
        sal_uInt16 nBaseCount = pGenoType->GetObjectBarCount();
        if ( nNo < nBaseCount )
            // The Super class comes first
            return pGenoType->GetObjectBarFeature( nNo );
        else
            nNo = nNo - nBaseCount;
    }

    assert( nNo<pImpData->aObjectBars.size() );

    return pImpData->aObjectBars[nNo]->nFeature;
}

bool SfxInterface::IsObjectBarVisible(sal_uInt16 nNo) const
{
    bool bGenoType = (pGenoType != 0 && !pGenoType->HasName());
    if ( bGenoType )
    {
        // Are there toolbars in the super class?
        sal_uInt16 nBaseCount = pGenoType->GetObjectBarCount();
        if ( nNo < nBaseCount )
            // The Super class comes first
            return pGenoType->IsObjectBarVisible( nNo );
        else
            nNo = nNo - nBaseCount;
    }

    assert( nNo<pImpData->aObjectBars.size() );

    return pImpData->aObjectBars[nNo]->bVisible;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
