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

#include <hintids.hxx>
#include <editeng/frmdiritem.hxx>
#include <editeng/protitem.hxx>
#include <tools/gen.hxx>
#include <com/sun/star/i18n/CharacterIteratorMode.hpp>
#include <fmtcntnt.hxx>
#include <fmtanchr.hxx>
#include <frmfmt.hxx>
#include <txtftn.hxx>
#include <ftnfrm.hxx>
#include <doc.hxx>
#include <docary.hxx>
#include <node.hxx>
#include <ndindex.hxx>
#include <numrule.hxx>
#include <swtable.hxx>
#include <ndtxt.hxx>
#include <pam.hxx>
#include <swcache.hxx>
#include <section.hxx>
#include <cntfrm.hxx>
#include <flyfrm.hxx>
#include <txtfrm.hxx>
#include <tabfrm.hxx>
#include <viewsh.hxx>
#include <paratr.hxx>
#include <ftnidx.hxx>
#include <fmtftn.hxx>
#include <fmthdft.hxx>
#include <frmatr.hxx>
#include <fmtautofmt.hxx>
#include <frmtool.hxx>
#include <pagefrm.hxx>
#include <node2lay.hxx>
#include <pagedesc.hxx>
#include <fmtpdsc.hxx>
#include <breakit.hxx>
#include <crsskip.hxx>
#include <SwStyleNameMapper.hxx>
#include <scriptinfo.hxx>
#include <rootfrm.hxx>
#include <istyleaccess.hxx>
#include <IDocumentListItems.hxx>
#include <DocumentSettingManager.hxx>
#include <IDocumentLinksAdministration.hxx>
#include <switerator.hxx>
#include "ndole.hxx"

using namespace ::com::sun::star::i18n;

TYPEINIT2( SwCntntNode, SwModify, SwIndexReg )

/*
 * Some local helper functions for the attribute set handle of a content node.
 * Since the attribute set of a content node may not be modified directly,
 * we always have to create a new SwAttrSet, do the modifications, and get
 * a new handle from the style access
 */

namespace AttrSetHandleHelper
{

void GetNewAutoStyle( boost::shared_ptr<const SfxItemSet>& mrpAttrSet,
                      const SwCntntNode& rNode,
                      SwAttrSet& rNewAttrSet )
{
    const SwAttrSet* pAttrSet = static_cast<const SwAttrSet*>(mrpAttrSet.get());
    if( rNode.GetModifyAtAttr() )
        const_cast<SwAttrSet*>(pAttrSet)->SetModifyAtAttr( 0 );
    IStyleAccess& rSA = pAttrSet->GetPool()->GetDoc()->GetIStyleAccess();
    mrpAttrSet = rSA.getAutomaticStyle( rNewAttrSet, rNode.IsTxtNode() ?
                                                     IStyleAccess::AUTO_STYLE_PARA :
                                                     IStyleAccess::AUTO_STYLE_NOTXT );
    const bool bSetModifyAtAttr = ((SwAttrSet*)mrpAttrSet.get())->SetModifyAtAttr( &rNode );
    rNode.SetModifyAtAttr( bSetModifyAtAttr );
}

void SetParent( boost::shared_ptr<const SfxItemSet>& mrpAttrSet,
                const SwCntntNode& rNode,
                const SwFmt* pParentFmt,
                const SwFmt* pConditionalFmt )
{
    const SwAttrSet* pAttrSet = static_cast<const SwAttrSet*>(mrpAttrSet.get());
    OSL_ENSURE( pAttrSet, "no SwAttrSet" );
    OSL_ENSURE( pParentFmt || !pConditionalFmt, "ConditionalFmt without ParentFmt?" );

    const SwAttrSet* pParentSet = pParentFmt ? &pParentFmt->GetAttrSet() : 0;

    if ( pParentSet != pAttrSet->GetParent() )
    {
        SwAttrSet aNewSet( *pAttrSet );
        aNewSet.SetParent( pParentSet );
        aNewSet.ClearItem( RES_FRMATR_STYLE_NAME );
        aNewSet.ClearItem( RES_FRMATR_CONDITIONAL_STYLE_NAME );
        OUString sVal;

        if ( pParentFmt )
        {
            SwStyleNameMapper::FillProgName( pParentFmt->GetName(), sVal, nsSwGetPoolIdFromName::GET_POOLID_TXTCOLL, true );
            const SfxStringItem aAnyFmtColl( RES_FRMATR_STYLE_NAME, sVal );
            aNewSet.Put( aAnyFmtColl );

            if ( pConditionalFmt != pParentFmt )
                SwStyleNameMapper::FillProgName( pConditionalFmt->GetName(), sVal, nsSwGetPoolIdFromName::GET_POOLID_TXTCOLL, true );

            const SfxStringItem aFmtColl( RES_FRMATR_CONDITIONAL_STYLE_NAME, sVal );
            aNewSet.Put( aFmtColl );
        }

        GetNewAutoStyle( mrpAttrSet, rNode, aNewSet );
    }
}

const SfxPoolItem* Put( boost::shared_ptr<const SfxItemSet>& mrpAttrSet,
                        const SwCntntNode& rNode,
                        const SfxPoolItem& rAttr )
{
    SwAttrSet aNewSet( (SwAttrSet&)*mrpAttrSet );
    const SfxPoolItem* pRet = aNewSet.Put( rAttr );
    if ( pRet )
        GetNewAutoStyle( mrpAttrSet, rNode, aNewSet );
    return pRet;
}

bool Put( boost::shared_ptr<const SfxItemSet>& mrpAttrSet, const SwCntntNode& rNode,
         const SfxItemSet& rSet )
{
    SwAttrSet aNewSet( (SwAttrSet&)*mrpAttrSet );

    // #i76273# Robust
    SfxItemSet* pStyleNames = 0;
    if ( SFX_ITEM_SET == rSet.GetItemState( RES_FRMATR_STYLE_NAME, false ) )
    {
        pStyleNames = new SfxItemSet( *aNewSet.GetPool(), RES_FRMATR_STYLE_NAME, RES_FRMATR_CONDITIONAL_STYLE_NAME );
        pStyleNames->Put( aNewSet );
    }

    const bool nRet = aNewSet.Put( rSet );

    // #i76273# Robust
    if ( pStyleNames )
    {
        aNewSet.Put( *pStyleNames );
        delete pStyleNames;
    }

    if ( nRet )
        GetNewAutoStyle( mrpAttrSet, rNode, aNewSet );

    return nRet;
}

bool Put_BC( boost::shared_ptr<const SfxItemSet>& mrpAttrSet,
            const SwCntntNode& rNode, const SfxPoolItem& rAttr,
            SwAttrSet* pOld, SwAttrSet* pNew )
{
    SwAttrSet aNewSet( (SwAttrSet&)*mrpAttrSet );

    // for a correct broadcast, we need to do a SetModifyAtAttr with the items
    // from aNewSet. The 'regular' SetModifyAtAttr is done in GetNewAutoStyle
    if( rNode.GetModifyAtAttr() )
        aNewSet.SetModifyAtAttr( &rNode );

    const bool nRet = aNewSet.Put_BC( rAttr, pOld, pNew );

    if ( nRet )
        GetNewAutoStyle( mrpAttrSet, rNode, aNewSet );

    return nRet;
}

bool Put_BC( boost::shared_ptr<const SfxItemSet>& mrpAttrSet,
            const SwCntntNode& rNode, const SfxItemSet& rSet,
            SwAttrSet* pOld, SwAttrSet* pNew )
{
    SwAttrSet aNewSet( (SwAttrSet&)*mrpAttrSet );

    // #i76273# Robust
    SfxItemSet* pStyleNames = 0;
    if ( SFX_ITEM_SET == rSet.GetItemState( RES_FRMATR_STYLE_NAME, false ) )
    {
        pStyleNames = new SfxItemSet( *aNewSet.GetPool(), RES_FRMATR_STYLE_NAME, RES_FRMATR_CONDITIONAL_STYLE_NAME );
        pStyleNames->Put( aNewSet );
    }

    // for a correct broadcast, we need to do a SetModifyAtAttr with the items
    // from aNewSet. The 'regular' SetModifyAtAttr is done in GetNewAutoStyle
    if( rNode.GetModifyAtAttr() )
        aNewSet.SetModifyAtAttr( &rNode );

    const bool nRet = aNewSet.Put_BC( rSet, pOld, pNew );

    // #i76273# Robust
    if ( pStyleNames )
    {
        aNewSet.Put( *pStyleNames );
        delete pStyleNames;
    }

    if ( nRet )
        GetNewAutoStyle( mrpAttrSet, rNode, aNewSet );

    return nRet;
}

sal_uInt16 ClearItem_BC( boost::shared_ptr<const SfxItemSet>& mrpAttrSet,
                     const SwCntntNode& rNode, sal_uInt16 nWhich,
                     SwAttrSet* pOld, SwAttrSet* pNew )
{
    SwAttrSet aNewSet( (SwAttrSet&)*mrpAttrSet );
    if( rNode.GetModifyAtAttr() )
        aNewSet.SetModifyAtAttr( &rNode );
    const sal_uInt16 nRet = aNewSet.ClearItem_BC( nWhich, pOld, pNew );
    if ( nRet )
        GetNewAutoStyle( mrpAttrSet, rNode, aNewSet );
    return nRet;
}

sal_uInt16 ClearItem_BC( boost::shared_ptr<const SfxItemSet>& mrpAttrSet,
                     const SwCntntNode& rNode,
                     sal_uInt16 nWhich1, sal_uInt16 nWhich2,
                     SwAttrSet* pOld, SwAttrSet* pNew )
{
    SwAttrSet aNewSet( (SwAttrSet&)*mrpAttrSet );
    if( rNode.GetModifyAtAttr() )
        aNewSet.SetModifyAtAttr( &rNode );
    const sal_uInt16 nRet = aNewSet.ClearItem_BC( nWhich1, nWhich2, pOld, pNew );
    if ( nRet )
        GetNewAutoStyle( mrpAttrSet, rNode, aNewSet );
    return nRet;
}

}

/** Returns the section level at the position given by aIndex.
 *
 * We use the following logic:
 * S = Start, E = End, C = CntntNode
 * Level   0 = E
 *         1 = S E
 *         2 = SC
 *
 * All EndNodes of the BaseSection have level 0
 * All StartNodes of the BaseSection have level 1
 */
sal_uInt16 SwNode::GetSectionLevel() const
{
    // EndNode of a BaseSection? They are always 0!
    if( IsEndNode() && 0 == pStartOfSection->StartOfSectionIndex() )
        return 0;

    sal_uInt16 nLevel;
    const SwNode* pNode = IsStartNode() ? this : pStartOfSection;
    for( nLevel = 1; 0 != pNode->StartOfSectionIndex(); ++nLevel )
        pNode = pNode->pStartOfSection;
    return IsEndNode() ? nLevel-1 : nLevel;
}

#ifdef DBG_UTIL
long SwNode::s_nSerial = 0;
#endif

SwNode::SwNode( const SwNodeIndex &rWhere, const sal_uInt8 nNdType )
    : nNodeType( nNdType )
    , nAFmtNumLvl( 0 )
    , bSetNumLSpace( false )
    , bIgnoreDontExpand( false)
#ifdef DBG_UTIL
    , m_nSerial( s_nSerial++)
#endif
    , pStartOfSection( 0 )
{
    if( rWhere.GetIndex() )
    {
        SwNodes& rNodes = const_cast<SwNodes&> (rWhere.GetNodes());
        SwNode* pNd = rNodes[ rWhere.GetIndex() -1 ];
        rNodes.InsertNode( this, rWhere );
        if( 0 == ( pStartOfSection = pNd->GetStartNode()) )
        {
            pStartOfSection = pNd->pStartOfSection;
            if( pNd->GetEndNode() )     // Skip EndNode ? Section
            {
                pNd = pStartOfSection;
                pStartOfSection = pNd->pStartOfSection;
            }
        }
    }
}

/** Inserts a node into the rNodes array at the rWhere position
 *
 * @param rNodes the variable array in that the node will be inserted
 * @param nPos position within the array where the node will be inserted
 * @param nNdType the type of node to insert
 */
SwNode::SwNode( SwNodes& rNodes, sal_uLong nPos, const sal_uInt8 nNdType )
    : nNodeType( nNdType )
    , nAFmtNumLvl( 0 )
    , bSetNumLSpace( false )
    , bIgnoreDontExpand( false)
#ifdef DBG_UTIL
    , m_nSerial( s_nSerial++)
#endif
    , pStartOfSection( 0 )
{
    if( nPos )
    {
        SwNode* pNd = rNodes[ nPos - 1 ];
        rNodes.InsertNode( this, nPos );
        if( 0 == ( pStartOfSection = pNd->GetStartNode()) )
        {
            pStartOfSection = pNd->pStartOfSection;
            if( pNd->GetEndNode() )     // Skip EndNode ? Section!
            {
                pNd = pStartOfSection;
                pStartOfSection = pNd->pStartOfSection;
            }
        }
    }
}

SwNode::~SwNode()
{
}

/// Find the TableNode in which it is located.
/// If we're not in a table: return 0
SwTableNode* SwNode::FindTableNode()
{
    if( IsTableNode() )
        return GetTableNode();
    SwStartNode* pTmp = pStartOfSection;
    while( !pTmp->IsTableNode() && pTmp->GetIndex() )
        pTmp = pTmp->pStartOfSection;
    return pTmp->GetTableNode();
}

/// Is the node located in the visible area of the Shell?
bool SwNode::IsInVisibleArea( SwViewShell* pSh ) const
{
    bool bRet = false;
    const SwCntntNode* pNd;

    if( ND_STARTNODE & nNodeType )
    {
        SwNodeIndex aIdx( *this );
        pNd = GetNodes().GoNext( &aIdx );
    }
    else if( ND_ENDNODE & nNodeType )
    {
        SwNodeIndex aIdx( *EndOfSectionNode() );
        pNd = GetNodes().GoPrevious( &aIdx );
    }
    else
        pNd = GetCntntNode();

    if( !pSh )
        // Get the Shell from the Doc
        GetDoc()->GetEditShell( &pSh );

    if( pSh )
    {
        const SwFrm* pFrm;
        if( pNd && 0 != ( pFrm = pNd->getLayoutFrm( pSh->GetLayout(), 0, 0, false ) ) )
        {

            if ( pFrm->IsInTab() )
                pFrm = pFrm->FindTabFrm();

            if( !pFrm->IsValid() )
                do
                {   pFrm = pFrm->FindPrev();
                } while ( pFrm && !pFrm->IsValid() );

            if( !pFrm || pSh->VisArea().IsOver( pFrm->Frm() ) )
                bRet = true;
        }
    }

    return bRet;
}

bool SwNode::IsInProtectSect() const
{
    const SwNode* pNd = ND_SECTIONNODE == nNodeType ? pStartOfSection : this;
    const SwSectionNode* pSectNd = pNd->FindSectionNode();
    return pSectNd && pSectNd->GetSection().IsProtectFlag();
}

/// Does the node contain anything protected?
/// I.e.: Area/Frame/Table rows/... including the Anchor for
/// Frames/Footnotes/...
bool SwNode::IsProtect() const
{
    const SwNode* pNd = ND_SECTIONNODE == nNodeType ? pStartOfSection : this;
    const SwStartNode* pSttNd = pNd->FindSectionNode();
    if( pSttNd && ((SwSectionNode*)pSttNd)->GetSection().IsProtectFlag() )
        return true;

    if( 0 != ( pSttNd = FindTableBoxStartNode() ) )
    {
        SwCntntFrm* pCFrm;
        if( IsCntntNode() && 0 != (pCFrm = ((SwCntntNode*)this)->getLayoutFrm( GetDoc()->GetCurrentLayout() ) ))
            return pCFrm->IsProtected();

        const SwTableBox* pBox = pSttNd->FindTableNode()->GetTable().
                                        GetTblBox( pSttNd->GetIndex() );
        //Robust #149568
        if( pBox && pBox->GetFrmFmt()->GetProtect().IsCntntProtected() )
            return true;
    }

    SwFrmFmt* pFlyFmt = GetFlyFmt();
    if( pFlyFmt )
    {
        if( pFlyFmt->GetProtect().IsCntntProtected() )
            return true;
        const SwFmtAnchor& rAnchor = pFlyFmt->GetAnchor();
        return rAnchor.GetCntntAnchor()
                ? rAnchor.GetCntntAnchor()->nNode.GetNode().IsProtect()
                : sal_False;
    }

    if( 0 != ( pSttNd = FindFootnoteStartNode() ) )
    {
        const SwTxtFtn* pTFtn = GetDoc()->GetFtnIdxs().SeekEntry(
                                SwNodeIndex( *pSttNd ) );
        if( pTFtn )
            return pTFtn->GetTxtNode().IsProtect();
    }

    return false;
}

/// Find the PageDesc that is used to format this node. If the Layout is available,
/// we search through that. Else we can only do it the hard way by searching onwards through the nodes.
const SwPageDesc* SwNode::FindPageDesc( bool bCalcLay,
                                        sal_uInt32* pPgDescNdIdx ) const
{
    if ( !GetNodes().IsDocNodes() )
    {
        return 0;
    }

    const SwPageDesc* pPgDesc = 0;

    const SwCntntNode* pNode;
    if( ND_STARTNODE & nNodeType )
    {
        SwNodeIndex aIdx( *this );
        pNode = GetNodes().GoNext( &aIdx );
    }
    else if( ND_ENDNODE & nNodeType )
    {
        SwNodeIndex aIdx( *EndOfSectionNode() );
        pNode = GetNodes().GoPrevious( &aIdx );
    }
    else
    {
        pNode = GetCntntNode();
        if( pNode )
            pPgDesc = ((SwFmtPageDesc&)pNode->GetAttr( RES_PAGEDESC )).GetPageDesc();
    }

    // Are we going through the layout?
    if( !pPgDesc )
    {
        const SwFrm* pFrm;
        const SwPageFrm* pPage;
        if( pNode && 0 != ( pFrm = pNode->getLayoutFrm( pNode->GetDoc()->GetCurrentLayout(), 0, 0, bCalcLay ) ) &&
            0 != ( pPage = pFrm->FindPageFrm() ) )
        {
            pPgDesc = pPage->GetPageDesc();
            if ( pPgDescNdIdx )
            {
                *pPgDescNdIdx = pNode->GetIndex();
            }
        }
    }

    if( !pPgDesc )
    {
        // Thus via the nodes array
        const SwDoc* pDoc = GetDoc();
        const SwNode* pNd = this;
        const SwStartNode* pSttNd;
        if( pNd->GetIndex() < GetNodes().GetEndOfExtras().GetIndex() &&
            0 != ( pSttNd = pNd->FindFlyStartNode() ) )
        {
            // Find the right Anchor first
            const SwFrmFmt* pFmt = 0;
            const SwFrmFmts& rFmts = *pDoc->GetSpzFrmFmts();
            sal_uInt16 n;

            for( n = 0; n < rFmts.size(); ++n )
            {
                SwFrmFmt* pFrmFmt = rFmts[ n ];
                const SwFmtCntnt& rCntnt = pFrmFmt->GetCntnt();
                if( rCntnt.GetCntntIdx() &&
                    &rCntnt.GetCntntIdx()->GetNode() == (SwNode*)pSttNd )
                {
                    pFmt = pFrmFmt;
                    break;
                }
            }

            if( pFmt )
            {
                const SwFmtAnchor* pAnchor = &pFmt->GetAnchor();
                if ((FLY_AT_PAGE != pAnchor->GetAnchorId()) &&
                    pAnchor->GetCntntAnchor() )
                {
                    pNd = &pAnchor->GetCntntAnchor()->nNode.GetNode();
                    const SwNode* pFlyNd = pNd->FindFlyStartNode();
                    while( pFlyNd )
                    {
                        // Get up through the Anchor
                        for( n = 0; n < rFmts.size(); ++n )
                        {
                            const SwFrmFmt* pFrmFmt = rFmts[ n ];
                            const SwNodeIndex* pIdx = pFrmFmt->GetCntnt().
                                                        GetCntntIdx();
                            if( pIdx && pFlyNd == &pIdx->GetNode() )
                            {
                                if( pFmt == pFrmFmt )
                                {
                                    pNd = pFlyNd;
                                    pFlyNd = 0;
                                    break;
                                }
                                pAnchor = &pFrmFmt->GetAnchor();
                                if ((FLY_AT_PAGE == pAnchor->GetAnchorId()) ||
                                    !pAnchor->GetCntntAnchor() )
                                {
                                    pFlyNd = 0;
                                    break;
                                }

                                pFlyNd = pAnchor->GetCntntAnchor()->nNode.
                                        GetNode().FindFlyStartNode();
                                break;
                            }
                        }
                        if( n >= rFmts.size() )
                        {
                            OSL_ENSURE( !this, "FlySection, but no Format found" );
                            return 0;
                        }
                    }
                }
            }
            // pNd should now contain the correct Anchor or it's still this
        }

        if( pNd->GetIndex() < GetNodes().GetEndOfExtras().GetIndex() )
        {
            if( pNd->GetIndex() > GetNodes().GetEndOfAutotext().GetIndex() )
            {
                pPgDesc = &pDoc->GetPageDesc( 0 );
                pNd = 0;
            }
            else
            {
                // Find the Body text node
                if( 0 != ( pSttNd = pNd->FindHeaderStartNode() ) ||
                    0 != ( pSttNd = pNd->FindFooterStartNode() ))
                {
                    // Then find this StartNode in the PageDescs
                    sal_uInt16 nId;
                    UseOnPage eAskUse;
                    if( SwHeaderStartNode == pSttNd->GetStartNodeType())
                    {
                        nId = RES_HEADER;
                        eAskUse = nsUseOnPage::PD_HEADERSHARE;
                    }
                    else
                    {
                        nId = RES_FOOTER;
                        eAskUse = nsUseOnPage::PD_FOOTERSHARE;
                    }

                    for( sal_uInt16 n = pDoc->GetPageDescCnt(); n && !pPgDesc; )
                    {
                        const SwPageDesc& rPgDsc = pDoc->GetPageDesc( --n );
                        const SwFrmFmt* pFmt = &rPgDsc.GetMaster();
                        int nStt = 0, nLast = 1;
                        if( !( eAskUse & rPgDsc.ReadUseOn() )) ++nLast;

                        for( ; nStt < nLast; ++nStt, pFmt = &rPgDsc.GetLeft() )
                        {
                            const SwFmtHeader& rHdFt = (SwFmtHeader&)
                                                    pFmt->GetFmtAttr( nId );
                            if( rHdFt.GetHeaderFmt() )
                            {
                                const SwFmtCntnt& rCntnt =
                                    rHdFt.GetHeaderFmt()->GetCntnt();
                                if( rCntnt.GetCntntIdx() &&
                                    &rCntnt.GetCntntIdx()->GetNode() ==
                                    (SwNode*)pSttNd )
                                {
                                    pPgDesc = &rPgDsc;
                                    break;
                                }
                            }
                        }
                    }

                    if( !pPgDesc )
                        pPgDesc = &pDoc->GetPageDesc( 0 );
                    pNd = 0;
                }
                else if( 0 != ( pSttNd = pNd->FindFootnoteStartNode() ))
                {
                    // the Anchor can only be in the Body text
                    const SwTxtFtn* pTxtFtn;
                    const SwFtnIdxs& rFtnArr = pDoc->GetFtnIdxs();
                    for( sal_uInt16 n = 0; n < rFtnArr.size(); ++n )
                        if( 0 != ( pTxtFtn = rFtnArr[ n ])->GetStartNode() &&
                            (SwNode*)pSttNd ==
                            &pTxtFtn->GetStartNode()->GetNode() )
                        {
                            pNd = &pTxtFtn->GetTxtNode();
                            break;
                        }
                }
                else
                {
                    // Can only be a page-bound Fly (or something newer).
                    // we can only return the standard here
                    OSL_ENSURE( pNd->FindFlyStartNode(),
                            "Where is this Node?" );

                    pPgDesc = &pDoc->GetPageDesc( 0 );
                    pNd = 0;
                }
            }
        }

        if( pNd )
        {
            SwFindNearestNode aInfo( *pNd );
            // Over all Nodes of all PageDescs
            const SfxPoolItem* pItem;
            sal_uInt32 i, nMaxItems = pDoc->GetAttrPool().GetItemCount2( RES_PAGEDESC );
            for( i = 0; i < nMaxItems; ++i )
                if( 0 != (pItem = pDoc->GetAttrPool().GetItem2( RES_PAGEDESC, i ) ) &&
                    ((SwFmtPageDesc*)pItem)->GetDefinedIn() )
                {
                    const SwModify* pMod = ((SwFmtPageDesc*)pItem)->GetDefinedIn();
                    if( pMod->ISA( SwCntntNode ) )
                        aInfo.CheckNode( *(SwCntntNode*)pMod );
                    else if( pMod->ISA( SwFmt ))
                        ((SwFmt*)pMod)->GetInfo( aInfo );
                }

            if( 0 != ( pNd = aInfo.GetFoundNode() ))
            {
                if( pNd->IsCntntNode() )
                    pPgDesc = ((SwFmtPageDesc&)pNd->GetCntntNode()->
                                GetAttr( RES_PAGEDESC )).GetPageDesc();
                else if( pNd->IsTableNode() )
                    pPgDesc = pNd->GetTableNode()->GetTable().
                            GetFrmFmt()->GetPageDesc().GetPageDesc();
                else if( pNd->IsSectionNode() )
                    pPgDesc = pNd->GetSectionNode()->GetSection().
                            GetFmt()->GetPageDesc().GetPageDesc();
                if ( pPgDescNdIdx )
                {
                    *pPgDescNdIdx = pNd->GetIndex();
                }
            }
            if( !pPgDesc )
                pPgDesc = &pDoc->GetPageDesc( 0 );
        }
    }
    return pPgDesc;
}

/// If the node is located in a Fly, we return it formatted accordingly
SwFrmFmt* SwNode::GetFlyFmt() const
{
    SwFrmFmt* pRet = 0;
    const SwNode* pSttNd = FindFlyStartNode();
    if( pSttNd )
    {
        if( IsCntntNode() )
        {
            SwCntntFrm* pFrm = SwIterator<SwCntntFrm,SwCntntNode>::FirstElement( *(SwCntntNode*)this );
            if( pFrm )
                pRet = pFrm->FindFlyFrm()->GetFmt();
        }
        if( !pRet )
        {
            // The hard way through the Doc is our last way out
            const SwFrmFmts& rFrmFmtTbl = *GetDoc()->GetSpzFrmFmts();
            for( sal_uInt16 n = 0; n < rFrmFmtTbl.size(); ++n )
            {
                SwFrmFmt* pFmt = rFrmFmtTbl[n];
                const SwFmtCntnt& rCntnt = pFmt->GetCntnt();
                if( rCntnt.GetCntntIdx() &&
                    &rCntnt.GetCntntIdx()->GetNode() == pSttNd )
                {
                    pRet = pFmt;
                    break;
                }
            }
        }
    }
    return pRet;
}

SwTableBox* SwNode::GetTblBox() const
{
    SwTableBox* pBox = 0;
    const SwNode* pSttNd = FindTableBoxStartNode();
    if( pSttNd )
        pBox = (SwTableBox*)pSttNd->FindTableNode()->GetTable().GetTblBox(
                                                    pSttNd->GetIndex() );
    return pBox;
}

SwStartNode* SwNode::FindSttNodeByType( SwStartNodeType eTyp )
{
    SwStartNode* pTmp = IsStartNode() ? (SwStartNode*)this : pStartOfSection;

    while( eTyp != pTmp->GetStartNodeType() && pTmp->GetIndex() )
        pTmp = pTmp->pStartOfSection;
    return eTyp == pTmp->GetStartNodeType() ? pTmp : 0;
}

const SwTxtNode* SwNode::FindOutlineNodeOfLevel( sal_uInt8 nLvl ) const
{
    const SwTxtNode* pRet = 0;
    const SwOutlineNodes& rONds = GetNodes().GetOutLineNds();
    if( MAXLEVEL > nLvl && !rONds.empty() )
    {
        sal_uInt16 nPos;
        SwNode* pNd = (SwNode*)this;
        bool bCheckFirst = false;
        if( !rONds.Seek_Entry( pNd, &nPos ))
        {
            if( nPos )
                nPos = nPos-1;
            else
                bCheckFirst = true;
        }

        if( bCheckFirst )
        {
            // The first OutlineNode comes after the one asking. Test if it points to the same node.
            // If not it's invalid.
            pRet = rONds[0]->GetTxtNode();

            const SwCntntNode* pCNd = GetCntntNode();

            Point aPt( 0, 0 );
            const SwFrm* pFrm = pRet->getLayoutFrm( pRet->GetDoc()->GetCurrentLayout(), &aPt, 0, false ),
                       * pMyFrm = pCNd ? pCNd->getLayoutFrm( pCNd->GetDoc()->GetCurrentLayout(), &aPt, 0, false ) : 0;
            const SwPageFrm* pPgFrm = pFrm ? pFrm->FindPageFrm() : 0;
            if( pPgFrm && pMyFrm &&
                pPgFrm->Frm().Top() > pMyFrm->Frm().Top() )
            {
                // The one asking precedes the Page, thus its invalid
                pRet = 0;
            }
        }
        else
        {
            // Or at the Field and get it from there!
            while( nPos &&
                   nLvl < ( pRet = rONds[nPos]->GetTxtNode() )
                    ->GetAttrOutlineLevel() - 1 )
                --nPos;

            if( !nPos )     // Get separately when 0
                pRet = rONds[0]->GetTxtNode();
        }
    }
    return pRet;
}

inline bool IsValidNextPrevNd( const SwNode& rNd )
{
    return ND_TABLENODE == rNd.GetNodeType() ||
           ( ND_CONTENTNODE & rNd.GetNodeType() ) ||
            ( ND_ENDNODE == rNd.GetNodeType() && rNd.StartOfSectionNode() &&
            ND_TABLENODE == rNd.StartOfSectionNode()->GetNodeType() );
}

sal_uInt8 SwNode::HasPrevNextLayNode() const
{
    // assumption: <this> node is a node inside the document nodes array section.

    sal_uInt8 nRet = 0;
    if( IsValidNextPrevNd( *this ))
    {
        SwNodeIndex aIdx( *this, -1 );
        // #i77805# - skip section start and end nodes
        while ( aIdx.GetNode().IsSectionNode() ||
                ( aIdx.GetNode().IsEndNode() &&
                  aIdx.GetNode().StartOfSectionNode()->IsSectionNode() ) )
        {
            --aIdx;
        }
        if( IsValidNextPrevNd( aIdx.GetNode() ))
            nRet |= ND_HAS_PREV_LAYNODE;
        // #i77805# - skip section start and end nodes
        aIdx = SwNodeIndex( *this, +1 );
        while ( aIdx.GetNode().IsSectionNode() ||
                ( aIdx.GetNode().IsEndNode() &&
                  aIdx.GetNode().StartOfSectionNode()->IsSectionNode() ) )
        {
            ++aIdx;
        }
        if( IsValidNextPrevNd( aIdx.GetNode() ))
            nRet |= ND_HAS_NEXT_LAYNODE;
    }
    return nRet;
}

SwStartNode::SwStartNode( const SwNodeIndex &rWhere, const sal_uInt8 nNdType,
                            SwStartNodeType eSttNd )
    : SwNode( rWhere, nNdType ), eSttNdTyp( eSttNd )
{
    if( !rWhere.GetIndex() )
    {
        SwNodes& rNodes = const_cast<SwNodes&> (rWhere.GetNodes());
        rNodes.InsertNode( this, rWhere );
        pStartOfSection = this;
    }
    // Just do this temporarily until the EndNode is inserted
    pEndOfSection = (SwEndNode*)this;
}

SwStartNode::SwStartNode( SwNodes& rNodes, sal_uLong nPos )
    : SwNode( rNodes, nPos, ND_STARTNODE ), eSttNdTyp( SwNormalStartNode )
{
    if( !nPos )
    {
        rNodes.InsertNode( this, nPos );
        pStartOfSection = this;
    }
    // Just do this temporarily until the EndNode is inserted
    pEndOfSection = (SwEndNode*)this;
}

void SwStartNode::CheckSectionCondColl() const
{
//FEATURE::CONDCOLL
    SwNodeIndex aIdx( *this );
    sal_uLong nEndIdx = EndOfSectionIndex();
    const SwNodes& rNds = GetNodes();
    SwCntntNode* pCNd;
    while( 0 != ( pCNd = rNds.GoNext( &aIdx )) && pCNd->GetIndex() < nEndIdx )
        pCNd->ChkCondColl();
//FEATURE::CONDCOLL
}

/** Insert a node into the array
 *
 * The StartOfSection pointer is set to the given node.
 *
 * The EndOfSection pointer of the corresponding start node is set to this node.
 *
 * @param rWhere position where the node shoul be inserted
 * @param rSttNd the start note of the section
 */

SwEndNode::SwEndNode( const SwNodeIndex &rWhere, SwStartNode& rSttNd )
    : SwNode( rWhere, ND_ENDNODE )
{
    pStartOfSection = &rSttNd;
    pStartOfSection->pEndOfSection = this;
}

SwEndNode::SwEndNode( SwNodes& rNds, sal_uLong nPos, SwStartNode& rSttNd )
    : SwNode( rNds, nPos, ND_ENDNODE )
{
    pStartOfSection = &rSttNd;
    pStartOfSection->pEndOfSection = this;
}

SwCntntNode::SwCntntNode( const SwNodeIndex &rWhere, const sal_uInt8 nNdType,
                            SwFmtColl *pColl )
    : SwModify( pColl ),     // CrsrsShell, FrameFmt,
    SwNode( rWhere, nNdType ),
    pCondColl( 0 ),
    mbSetModifyAtAttr( false )
{
}

SwCntntNode::~SwCntntNode()
{
    // The base class SwClient of SwFrm excludes itself from the dependency list!
    // Thus, we need to delete all Frames in the dependency list.
    DelFrms(false);

    delete pCondColl;

    if ( mpAttrSet.get() && mbSetModifyAtAttr )
        ((SwAttrSet*)mpAttrSet.get())->SetModifyAtAttr( 0 );
}

void SwCntntNode::Modify( const SfxPoolItem* pOldValue, const SfxPoolItem* pNewValue )
{
    sal_uInt16 nWhich = pOldValue ? pOldValue->Which() :
                    pNewValue ? pNewValue->Which() : 0 ;

    switch( nWhich )
    {
    case RES_OBJECTDYING :
        if (pNewValue)
        {
            SwFmt * pFmt = (SwFmt *) ((SwPtrMsgPoolItem *)pNewValue)->pObject;

            // Do not mangle pointers if it is the upper-most format!
            if( GetRegisteredIn() == pFmt )
            {
                if( pFmt->GetRegisteredIn() )
                {
                    // If Parent, register anew in the new Parent
                    ((SwModify*)pFmt->GetRegisteredIn())->Add( this );
                    if ( GetpSwAttrSet() )
                        AttrSetHandleHelper::SetParent( mpAttrSet, *this, GetFmtColl(), GetFmtColl() );
                }
                else
                {
                    // Else register anyways when dying
                    ((SwModify*)GetRegisteredIn())->Remove( this );
                    if ( GetpSwAttrSet() )
                        AttrSetHandleHelper::SetParent( mpAttrSet, *this, 0, 0 );
                }
            }
        }
        break;

    case RES_FMT_CHG:
        // If the Format parent was switched, register the Attrset at the new one
        // Skip own Modify!
        if( GetpSwAttrSet() && pNewValue &&
            ((SwFmtChg*)pNewValue)->pChangedFmt == GetRegisteredIn() )
        {
            // Attach Set to the new parent
            AttrSetHandleHelper::SetParent( mpAttrSet, *this, GetFmtColl(), GetFmtColl() );
        }
        break;

//FEATURE::CONDCOLL
    case RES_CONDCOLL_CONDCHG:
        if( ((SwCondCollCondChg*)pNewValue)->pChangedFmt == GetRegisteredIn() &&
            &GetNodes() == &GetDoc()->GetNodes() )
        {
            ChkCondColl();
        }
        return ;    // Do not pass through to the base class/Frames
//FEATURE::CONDCOLL

    case RES_ATTRSET_CHG:
        if (GetNodes().IsDocNodes() && IsTxtNode() && pOldValue)
        {
            if( SFX_ITEM_SET == ((SwAttrSetChg*)pOldValue)->GetChgSet()->GetItemState(
                RES_CHRATR_HIDDEN, false ) )
            {
                ((SwTxtNode*)this)->SetCalcHiddenCharFlags();
            }
        }
        break;

    case RES_UPDATE_ATTR:
        if( GetNodes().IsDocNodes() && IsTxtNode() )
        {
            const sal_uInt16 nTmp = ((SwUpdateAttr*)pNewValue)->getWhichAttr();
            if ( RES_ATTRSET_CHG == nTmp )
            {
                // TODO: anybody wants to do some optimization here?
                ((SwTxtNode*)this)->SetCalcHiddenCharFlags();
            }
        }
        break;
    }

    NotifyClients( pOldValue, pNewValue );
}

bool SwCntntNode::InvalidateNumRule()
{
    SwNumRule* pRule = 0;
    const SfxPoolItem* pItem;
    if( GetNodes().IsDocNodes() &&
        0 != ( pItem = GetNoCondAttr( RES_PARATR_NUMRULE, true )) &&
        !((SwNumRuleItem*)pItem)->GetValue().isEmpty() &&
        0 != (pRule = GetDoc()->FindNumRulePtr(
                                ((SwNumRuleItem*)pItem)->GetValue() ) ) )
    {
        pRule->SetInvalidRule( true );
    }
    return 0 != pRule;
}

SwCntntFrm *SwCntntNode::getLayoutFrm( const SwRootFrm* _pRoot,
    const Point* pPoint, const SwPosition *pPos, const bool bCalcFrm ) const
{
    return (SwCntntFrm*) ::GetFrmOfModify( _pRoot, *(SwModify*)this, FRM_CNTNT,
                                            pPoint, pPos, bCalcFrm );
}

SwRect SwCntntNode::FindLayoutRect( const bool bPrtArea, const Point* pPoint,
                                    const bool bCalcFrm ) const
{
    SwRect aRet;
    SwCntntFrm* pFrm = (SwCntntFrm*)::GetFrmOfModify( 0, *(SwModify*)this,
                                            FRM_CNTNT, pPoint, 0, bCalcFrm );
    if( pFrm )
        aRet = bPrtArea ? pFrm->Prt() : pFrm->Frm();
    return aRet;
}

SwRect SwCntntNode::FindPageFrmRect( const bool bPrtArea, const Point* pPoint,
                                    const bool bCalcFrm ) const
{
    SwRect aRet;
    SwFrm* pFrm = ::GetFrmOfModify( 0, *(SwModify*)this,
                                            FRM_CNTNT, pPoint, 0, bCalcFrm );
    if( pFrm && 0 != ( pFrm = pFrm->FindPageFrm() ))
        aRet = bPrtArea ? pFrm->Prt() : pFrm->Frm();
    return aRet;
}

sal_Int32 SwCntntNode::Len() const { return 0; }

SwFmtColl *SwCntntNode::ChgFmtColl( SwFmtColl *pNewColl )
{
    OSL_ENSURE( pNewColl, "Collectionpointer is 0." );
    SwFmtColl *pOldColl = GetFmtColl();

    if( pNewColl != pOldColl )
    {
        pNewColl->Add( this );

        // Set the Parent of out AutoAttributes to the new Collection
        if( GetpSwAttrSet() )
            AttrSetHandleHelper::SetParent( mpAttrSet, *this, pNewColl, pNewColl );

//FEATURE::CONDCOLL
        // TODO: HACK: We need to recheck this condition according to the new template!
        if( true /*pNewColl */ )
        {
            SetCondFmtColl( 0 );
        }
//FEATURE::CONDCOLL

        if( !IsModifyLocked() )
        {
            SwFmtChg aTmp1( pOldColl );
            SwFmtChg aTmp2( pNewColl );
            SwCntntNode::Modify( &aTmp1, &aTmp2 );
        }
    }
    if ( IsInCache() )
    {
        SwFrm::GetCache().Delete( this );
        SetInCache( false );
    }
    return pOldColl;
}

bool SwCntntNode::GoNext(SwIndex * pIdx, sal_uInt16 nMode ) const
{
    bool bRet = true;
    if( pIdx->GetIndex() < Len() )
    {
        if( !IsTxtNode() )
            ++(*pIdx);
        else
        {
            const SwTxtNode& rTNd = *GetTxtNode();
            sal_Int32 nPos = pIdx->GetIndex();
            if( g_pBreakIt->GetBreakIter().is() )
            {
                sal_Int32 nDone = 0;
                sal_uInt16 nItrMode = ( CRSR_SKIP_CELLS & nMode ) ?
                                        CharacterIteratorMode::SKIPCELL :
                                        CharacterIteratorMode::SKIPCONTROLCHARACTER;
                nPos = g_pBreakIt->GetBreakIter()->nextCharacters( rTNd.GetTxt(), nPos,
                                   g_pBreakIt->GetLocale( rTNd.GetLang( nPos ) ),
                                   nItrMode, 1, nDone );

                // Check if nPos is inside hidden text range:
                if ( CRSR_SKIP_HIDDEN & nMode )
                {
                    sal_Int32 nHiddenStart;
                    sal_Int32 nHiddenEnd;
                    SwScriptInfo::GetBoundsOfHiddenRange( rTNd, nPos, nHiddenStart, nHiddenEnd );
                    if ( nHiddenStart != COMPLETE_STRING && nHiddenStart != nPos )
                         nPos = nHiddenEnd;
                }

                if( 1 == nDone )
                    *pIdx = nPos;
                else
                    bRet = false;
            }
            else if (nPos < rTNd.GetTxt().getLength())
                ++(*pIdx);
            else
                bRet = false;
        }
    }
    else
        bRet = false;
    return bRet;
}

bool SwCntntNode::GoPrevious(SwIndex * pIdx, sal_uInt16 nMode ) const
{
    bool bRet = true;
    if( pIdx->GetIndex() > 0 )
    {
        if( !IsTxtNode() )
            (*pIdx)--;
        else
        {
            const SwTxtNode& rTNd = *GetTxtNode();
            sal_Int32 nPos = pIdx->GetIndex();
            if( g_pBreakIt->GetBreakIter().is() )
            {
                sal_Int32 nDone = 0;
                sal_uInt16 nItrMode = ( CRSR_SKIP_CELLS & nMode ) ?
                                        CharacterIteratorMode::SKIPCELL :
                                        CharacterIteratorMode::SKIPCONTROLCHARACTER;
                nPos = g_pBreakIt->GetBreakIter()->previousCharacters( rTNd.GetTxt(), nPos,
                                   g_pBreakIt->GetLocale( rTNd.GetLang( nPos ) ),
                                   nItrMode, 1, nDone );

                // Check if nPos is inside hidden text range:
                if ( CRSR_SKIP_HIDDEN & nMode )
                {
                    sal_Int32 nHiddenStart;
                    sal_Int32 nHiddenEnd;
                    SwScriptInfo::GetBoundsOfHiddenRange( rTNd, nPos, nHiddenStart, nHiddenEnd );
                    if ( nHiddenStart != COMPLETE_STRING )
                         nPos = nHiddenStart;
                }

                if( 1 == nDone )
                    *pIdx = nPos;
                else
                    bRet = false;
            }
            else if( nPos )
                (*pIdx)--;
            else
                bRet = false;
        }
    }
    else
        bRet = false;
    return bRet;
}

/**
 * Creates all Views for the Doc for this Node.
 * The created ContentFrames are attached to the corresponding Layout.
 */
void SwCntntNode::MakeFrms( SwCntntNode& rNode )
{
    OSL_ENSURE( &rNode != this,
            "No ContentNode or CopyNode and new Node identical." );

    if( !GetDepends() || &rNode == this )   // Do we actually have Frames?
        return;

    SwFrm *pFrm, *pNew;
    SwLayoutFrm *pUpper;
    // Create Frames for Nodes which come after the Table?
    OSL_ENSURE( FindTableNode() == rNode.FindTableNode(), "Table confusion" );

    SwNode2Layout aNode2Layout( *this, rNode.GetIndex() );

    while( 0 != (pUpper = aNode2Layout.UpperFrm( pFrm, rNode )) )
    {
        pNew = rNode.MakeFrm( pUpper );
        pNew->Paste( pUpper, pFrm );
        // #i27138#
        // notify accessibility paragraphs objects about changed
        // CONTENT_FLOWS_FROM/_TO relation.
        // Relation CONTENT_FLOWS_FROM for next paragraph will change
        // and relation CONTENT_FLOWS_TO for previous paragraph will change.
        if ( pNew->IsTxtFrm() )
        {
            SwViewShell* pViewShell( pNew->getRootFrm()->GetCurrShell() );
            if ( pViewShell && pViewShell->GetLayout() &&
                 pViewShell->GetLayout()->IsAnyShellAccessible() )
            {
                pViewShell->InvalidateAccessibleParaFlowRelation(
                            dynamic_cast<SwTxtFrm*>(pNew->FindNextCnt( true )),
                            dynamic_cast<SwTxtFrm*>(pNew->FindPrevCnt( true )) );
            }
        }
    }
}

/**
 * Deletes all Views from the Doc for this Node.
 * The ContentFrames are removed from the corresponding Layout.
 *
 * An input param to identify if the acc table should be disposed.
 */
void SwCntntNode::DelFrms( bool bIsDisposeAccTable )
{
    if( !GetDepends() )
        return;

    SwIterator<SwCntntFrm,SwCntntNode> aIter( *this );
    for( SwCntntFrm* pFrm = aIter.First(); pFrm; pFrm = aIter.Next() )
    {
        // #i27138#
        // notify accessibility paragraphs objects about changed
        // CONTENT_FLOWS_FROM/_TO relation.
        // Relation CONTENT_FLOWS_FROM for current next paragraph will change
        // and relation CONTENT_FLOWS_TO for current previous paragraph will change.
        if ( pFrm->IsTxtFrm() )
        {
            SwViewShell* pViewShell( pFrm->getRootFrm()->GetCurrShell() );
            if ( pViewShell && pViewShell->GetLayout() &&
                 pViewShell->GetLayout()->IsAnyShellAccessible() )
            {
                pViewShell->InvalidateAccessibleParaFlowRelation(
                            dynamic_cast<SwTxtFrm*>(pFrm->FindNextCnt( true )),
                            dynamic_cast<SwTxtFrm*>(pFrm->FindPrevCnt( true )) );
            }
        }

        if( pFrm->IsFollow() )
        {
            SwCntntFrm* pMaster = (SwTxtFrm*)pFrm->FindMaster();
            pMaster->SetFollow( pFrm->GetFollow() );
        }
        pFrm->SetFollow( 0 );//So it doesn't get funny ideas.
                                //Otherwise it could be possible that a follow
                                //gets destroyed before its master. Following
                                //the now invalid pointer will then lead to an
                                //illegal memory access. The chain can be
                                //crushed here because we'll destroy all of it
                                //anyway.

        if( pFrm->GetUpper() && pFrm->IsInFtn() && !pFrm->GetIndNext() &&
            !pFrm->GetIndPrev() )
        {
            SwFtnFrm *pFtn = pFrm->FindFtnFrm();
            OSL_ENSURE( pFtn, "You promised a FtnFrm?" );
            SwCntntFrm* pCFrm;
            if( !pFtn->GetFollow() && !pFtn->GetMaster() &&
                0 != ( pCFrm = pFtn->GetRefFromAttr()) && pCFrm->IsFollow() )
            {
                OSL_ENSURE( pCFrm->IsTxtFrm(), "NoTxtFrm has Footnote?" );
                ((SwTxtFrm*)pCFrm->FindMaster())->Prepare( PREP_FTN_GONE );
            }
        }
        //Set acc table dispose state
        pFrm->SetAccTableDispose( bIsDisposeAccTable );
        pFrm->Cut();
        //Set acc table dispose state to default value
        pFrm->SetAccTableDispose( true );
        delete pFrm;
    }

    if( bIsDisposeAccTable && IsTxtNode() )
    {
        static_cast<SwTxtNode *>(this)->DelFrms_TxtNodePart();
    }
}

SwCntntNode *SwCntntNode::JoinNext()
{
    return this;
}

SwCntntNode *SwCntntNode::JoinPrev()
{
    return this;
}

/// Get info from Modify
bool SwCntntNode::GetInfo( SfxPoolItem& rInfo ) const
{
    switch( rInfo.Which() )
    {
    case RES_AUTOFMT_DOCNODE:
        if( &GetNodes() == ((SwAutoFmtGetDocNode&)rInfo).pNodes )
        {
            ((SwAutoFmtGetDocNode&)rInfo).pCntntNode = this;
            return false;
        }
        break;

    case RES_FINDNEARESTNODE:
        if( ((SwFmtPageDesc&)GetAttr( RES_PAGEDESC )).GetPageDesc() )
            ((SwFindNearestNode&)rInfo).CheckNode( *this );
        return true;

    case RES_CONTENT_VISIBLE:
        {
            ((SwPtrMsgPoolItem&)rInfo).pObject =
                SwIterator<SwFrm,SwCntntNode>::FirstElement(*this);
        }
        return false;
    }

    return SwModify::GetInfo( rInfo );
}

/// @param rAttr the attribute to set
bool SwCntntNode::SetAttr(const SfxPoolItem& rAttr )
{
    if( !GetpSwAttrSet() ) // Have the Nodes created by the corresponding AttrSets
        NewAttrSet( GetDoc()->GetAttrPool() );

    OSL_ENSURE( GetpSwAttrSet(), "Why did't we create an AttrSet?");

    if ( IsInCache() )
    {
        SwFrm::GetCache().Delete( this );
        SetInCache( false );
    }

    bool bRet = false;
    // If Modify is locked, we do not send any Modifys
    if( IsModifyLocked() ||
        ( !GetDepends() &&  RES_PARATR_NUMRULE != rAttr.Which() ))
    {
        bRet = 0 != AttrSetHandleHelper::Put( mpAttrSet, *this, rAttr );
    }
    else
    {
        SwAttrSet aOld( *GetpSwAttrSet()->GetPool(), GetpSwAttrSet()->GetRanges() ),
                  aNew( *GetpSwAttrSet()->GetPool(), GetpSwAttrSet()->GetRanges() );
        if( ( bRet = AttrSetHandleHelper::Put_BC( mpAttrSet, *this, rAttr, &aOld, &aNew ) ) )
        {
            SwAttrSetChg aChgOld( *GetpSwAttrSet(), aOld );
            SwAttrSetChg aChgNew( *GetpSwAttrSet(), aNew );
            ModifyNotification( &aChgOld, &aChgNew ); // Send all changed ones
        }
    }
    return bRet;
}

#include <svl/itemiter.hxx>

bool SwCntntNode::SetAttr( const SfxItemSet& rSet )
{
    if ( IsInCache() )
    {
        SwFrm::GetCache().Delete( this );
        SetInCache( false );
    }

    const SfxPoolItem* pFnd = 0;
    if( SFX_ITEM_SET == rSet.GetItemState( RES_AUTO_STYLE, false, &pFnd ) )
    {
        OSL_ENSURE( rSet.Count() == 1, "SetAutoStyle mixed with other attributes?!" );
        const SwFmtAutoFmt* pTmp = static_cast<const SwFmtAutoFmt*>(pFnd);

        // If there already is an attribute set (usually containing a numbering
        // item), we have to merge the attribute of the new set into the old set:
        bool bSetParent = true;
        if ( GetpSwAttrSet() )
        {
            bSetParent = false;
            AttrSetHandleHelper::Put( mpAttrSet, *this, *pTmp->GetStyleHandle() );
        }
        else
        {
            mpAttrSet = pTmp->GetStyleHandle();
        }

        if ( bSetParent )
        {
            // If the content node has a conditional style, we have to set the
            // string item containing the correct conditional style name (the
            // style name property has already been set during the import!)
            // In case we do not have a conditional style, we make use of the
            // fact that nobody else uses the attribute set behind the handle.
            // FME 2007-07-10 #i78124# If autostyle does not have a parent,
            // the string is empty.
            const SfxPoolItem* pNameItem = 0;
            if ( 0 != GetCondFmtColl() ||
                 SFX_ITEM_SET != mpAttrSet->GetItemState( RES_FRMATR_STYLE_NAME, false, &pNameItem ) ||
                 static_cast<const SfxStringItem*>(pNameItem)->GetValue().isEmpty() )
                AttrSetHandleHelper::SetParent( mpAttrSet, *this, &GetAnyFmtColl(), GetFmtColl() );
            else
                const_cast<SfxItemSet*>(mpAttrSet.get())->SetParent( &GetFmtColl()->GetAttrSet() );
        }

        return true;
    }

    if( !GetpSwAttrSet() ) // Have the AttrsSets created by the corresponding Nodes
        NewAttrSet( GetDoc()->GetAttrPool() );

    bool bRet = false;
    // If Modify is locked, do not send any Modifys
    if ( IsModifyLocked() ||
         ( !GetDepends() &&
           SFX_ITEM_SET != rSet.GetItemState( RES_PARATR_NUMRULE, false ) ) )
    {
        // Some special treatment for Attributes
        bRet = AttrSetHandleHelper::Put( mpAttrSet, *this, rSet );
    }
    else
    {
        SwAttrSet aOld( *GetpSwAttrSet()->GetPool(), GetpSwAttrSet()->GetRanges() ),
                  aNew( *GetpSwAttrSet()->GetPool(), GetpSwAttrSet()->GetRanges() );
        if( (bRet = AttrSetHandleHelper::Put_BC( mpAttrSet, *this, rSet, &aOld, &aNew )) )
        {
            // Some special treatment for Attributes
            SwAttrSetChg aChgOld( *GetpSwAttrSet(), aOld );
            SwAttrSetChg aChgNew( *GetpSwAttrSet(), aNew );
            ModifyNotification( &aChgOld, &aChgNew ); // Send out all changed ones
        }
    }
    return bRet;
}

// With nWhich it takes the Hint from the Delta array
bool SwCntntNode::ResetAttr( sal_uInt16 nWhich1, sal_uInt16 nWhich2 )
{
    if( !GetpSwAttrSet() )
        return false;

    if ( IsInCache() )
    {
        SwFrm::GetCache().Delete( this );
        SetInCache( false );
    }

    // If Modify is locked, do not send out any Modifys
    if( IsModifyLocked() )
    {
        sal_uInt16 nDel = 0;
        if ( !nWhich2 || nWhich2 < nWhich1 )
        {
            std::vector<sal_uInt16> aClearWhichIds;
            aClearWhichIds.push_back( nWhich1 );
            nDel = ClearItemsFromAttrSet( aClearWhichIds );
        }
        else
            nDel = AttrSetHandleHelper::ClearItem_BC( mpAttrSet, *this, nWhich1, nWhich2, 0, 0 );

        if( !GetpSwAttrSet()->Count() ) // Empt? Delete
            mpAttrSet.reset();
        return 0 != nDel;
    }

    // No valid area defined?
    if( !nWhich2 || nWhich2 < nWhich1 )
        nWhich2 = nWhich1; // Then set only this Item to 1st Id

    SwAttrSet aOld( *GetpSwAttrSet()->GetPool(), GetpSwAttrSet()->GetRanges() ),
              aNew( *GetpSwAttrSet()->GetPool(), GetpSwAttrSet()->GetRanges() );
    bool bRet = 0 != AttrSetHandleHelper::ClearItem_BC( mpAttrSet, *this, nWhich1, nWhich2, &aOld, &aNew );

    if( bRet )
    {
        SwAttrSetChg aChgOld( *GetpSwAttrSet(), aOld );
        SwAttrSetChg aChgNew( *GetpSwAttrSet(), aNew );
        ModifyNotification( &aChgOld, &aChgNew ); // All changed ones are sent

        if( !GetpSwAttrSet()->Count() ) // Empty?, delete it
            mpAttrSet.reset();
    }
    return bRet;
}

bool SwCntntNode::ResetAttr( const std::vector<sal_uInt16>& rWhichArr )
{
    if( !GetpSwAttrSet() )
        return false;

    if ( IsInCache() )
    {
        SwFrm::GetCache().Delete( this );
        SetInCache( false );
    }

    // If Modify is locked, do not send out any Modifys
    sal_uInt16 nDel = 0;
    if( IsModifyLocked() )
    {
        std::vector<sal_uInt16> aClearWhichIds(rWhichArr);
        nDel = ClearItemsFromAttrSet( aClearWhichIds );
    }
    else
    {
        SwAttrSet aOld( *GetpSwAttrSet()->GetPool(), GetpSwAttrSet()->GetRanges() ),
                  aNew( *GetpSwAttrSet()->GetPool(), GetpSwAttrSet()->GetRanges() );

        std::vector<sal_uInt16>::const_iterator it;
        for ( it = rWhichArr.begin(); it != rWhichArr.end(); ++it )
            if( AttrSetHandleHelper::ClearItem_BC( mpAttrSet, *this, *it, &aOld, &aNew ))
                ++nDel;

        if( nDel )
        {
            SwAttrSetChg aChgOld( *GetpSwAttrSet(), aOld );
            SwAttrSetChg aChgNew( *GetpSwAttrSet(), aNew );
            ModifyNotification( &aChgOld, &aChgNew ); // All changed ones are sent
        }
    }
    if( !GetpSwAttrSet()->Count() ) // Empty?, delete it
        mpAttrSet.reset();
    return 0 != nDel ;
}

sal_uInt16 SwCntntNode::ResetAllAttr()
{
    if( !GetpSwAttrSet() )
        return 0;

    if ( IsInCache() )
    {
        SwFrm::GetCache().Delete( this );
        SetInCache( false );
    }

    // If Modify is locked, do not send out any Modifys
    if( IsModifyLocked() )
    {
        std::vector<sal_uInt16> aClearWhichIds;
        aClearWhichIds.push_back(0);
        sal_uInt16 nDel = ClearItemsFromAttrSet( aClearWhichIds );
        if( !GetpSwAttrSet()->Count() ) // Empty? Delete
            mpAttrSet.reset();
        return nDel;
    }

    SwAttrSet aOld( *GetpSwAttrSet()->GetPool(), GetpSwAttrSet()->GetRanges() ),
              aNew( *GetpSwAttrSet()->GetPool(), GetpSwAttrSet()->GetRanges() );
    bool bRet = 0 != AttrSetHandleHelper::ClearItem_BC( mpAttrSet, *this, 0, &aOld, &aNew );

    if( bRet )
    {
        SwAttrSetChg aChgOld( *GetpSwAttrSet(), aOld );
        SwAttrSetChg aChgNew( *GetpSwAttrSet(), aNew );
        ModifyNotification( &aChgOld, &aChgNew ); // All changed ones are sent

        if( !GetpSwAttrSet()->Count() ) // Empty? Delete
            mpAttrSet.reset();
    }
    return aNew.Count();
}

bool SwCntntNode::GetAttr( SfxItemSet& rSet, bool bInParent ) const
{
    if( rSet.Count() )
        rSet.ClearItem();

    const SwAttrSet& rAttrSet = GetSwAttrSet();
    if( bInParent )
        return rSet.Set( rAttrSet, true ) ? sal_True : sal_False;

    rSet.Put( rAttrSet );
    return rSet.Count() ? sal_True : sal_False;
}

sal_uInt16 SwCntntNode::ClearItemsFromAttrSet( const std::vector<sal_uInt16>& rWhichIds )
{
    sal_uInt16 nRet = 0;
    if ( 0 == rWhichIds.size() )
        return nRet;

    OSL_ENSURE( GetpSwAttrSet(), "no item set" );
    SwAttrSet aNewAttrSet( *GetpSwAttrSet() );
    for ( std::vector<sal_uInt16>::const_iterator aIter = rWhichIds.begin();
          aIter != rWhichIds.end();
          ++aIter )
    {
        nRet = nRet + aNewAttrSet.ClearItem( *aIter );
    }
    if ( nRet )
        AttrSetHandleHelper::GetNewAutoStyle( mpAttrSet, *this, aNewAttrSet );

    return nRet;
}

const SfxPoolItem* SwCntntNode::GetNoCondAttr( sal_uInt16 nWhich,
                                               bool bInParents ) const
{
    const SfxPoolItem* pFnd = 0;
    if( pCondColl && pCondColl->GetRegisteredIn() )
    {
        if( !GetpSwAttrSet() || ( SFX_ITEM_SET != GetpSwAttrSet()->GetItemState(
                    nWhich, false, &pFnd ) && bInParents ))
            ((SwFmt*)GetRegisteredIn())->GetItemState( nWhich, bInParents, &pFnd );
    }
    // undo change of issue #i51029#
    // Note: <GetSwAttrSet()> returns <mpAttrSet>, if set, otherwise it returns
    //       the attribute set of the paragraph style, which is valid for the
    //       content node - see file <node.hxx>
    else
    {
        GetSwAttrSet().GetItemState( nWhich, bInParents, &pFnd );
    }
    return pFnd;
}

static bool lcl_CheckMaxLength(SwNode const& rPrev, SwNode const& rNext)
{
    if (rPrev.GetNodeType() != rNext.GetNodeType())
    {
        return false;
    }
    if (!rPrev.IsTxtNode())
    {
        return true;
    }

    // Check if a node can contain the other (order is not significant)
    return static_cast<const SwTxtNode&>(rPrev).GetSpaceLeft() >
           static_cast<const SwTxtNode&>(rNext).Len();
}

/// Can we join two Nodes?
/// We can return the 2nd position in pIdx.
bool SwCntntNode::CanJoinNext( SwNodeIndex* pIdx ) const
{
    const SwNodes& rNds = GetNodes();
    SwNodeIndex aIdx( *this, 1 );

    const SwNode* pNd = this;
    while( aIdx < rNds.Count()-1 &&
        (( pNd = &aIdx.GetNode())->IsSectionNode() ||
            ( pNd->IsEndNode() && pNd->StartOfSectionNode()->IsSectionNode() )))
        ++aIdx;

    if (rNds.Count()-1 == aIdx.GetIndex())
        return false;
    if (!lcl_CheckMaxLength(*this, *pNd))
    {
        return false;
    }
    if( pIdx )
        *pIdx = aIdx;
    return true;
}

/// Can we join two Nodes?
/// We can return the 2nd position in pIdx.
bool SwCntntNode::CanJoinPrev( SwNodeIndex* pIdx ) const
{
    SwNodeIndex aIdx( *this, -1 );

    const SwNode* pNd = this;
    while( aIdx.GetIndex() &&
        (( pNd = &aIdx.GetNode())->IsSectionNode() ||
            ( pNd->IsEndNode() && pNd->StartOfSectionNode()->IsSectionNode() )))
        aIdx--;

    if (0 == aIdx.GetIndex())
        return false;
    if (!lcl_CheckMaxLength(*pNd, *this))
    {
        return false;
    }
    if( pIdx )
        *pIdx = aIdx;
    return true;
}

//FEATURE::CONDCOLL
void SwCntntNode::SetCondFmtColl( SwFmtColl* pColl )
{
    if( (!pColl && pCondColl) || ( pColl && !pCondColl ) ||
        ( pColl && pColl != pCondColl->GetRegisteredIn() ) )
    {
        SwFmtColl* pOldColl = GetCondFmtColl();
        delete pCondColl;
        if( pColl )
            pCondColl = new SwDepend( this, pColl );
        else
            pCondColl = 0;

        if( GetpSwAttrSet() )
        {
            AttrSetHandleHelper::SetParent( mpAttrSet, *this, &GetAnyFmtColl(), GetFmtColl() );
        }

        if( !IsModifyLocked() )
        {
            SwFmtChg aTmp1( pOldColl ? pOldColl : GetFmtColl() );
            SwFmtChg aTmp2( pColl ? pColl : GetFmtColl() );
            NotifyClients( &aTmp1, &aTmp2 );
        }
        if( IsInCache() )
        {
            SwFrm::GetCache().Delete( this );
            SetInCache( false );
        }
    }
}

bool SwCntntNode::IsAnyCondition( SwCollCondition& rTmp ) const
{
    const SwNodes& rNds = GetNodes();
    {
        int nCond = 0;
        const SwStartNode* pSttNd = StartOfSectionNode();
        while( pSttNd )
        {
            switch( pSttNd->GetNodeType() )
            {
            case ND_TABLENODE:      nCond = PARA_IN_TABLEBODY; break;
            case ND_SECTIONNODE:    nCond = PARA_IN_SECTION; break;

            default:
                switch( pSttNd->GetStartNodeType() )
                {
                case SwTableBoxStartNode:
                    {
                        nCond = PARA_IN_TABLEBODY;
                        const SwTableNode* pTblNd = pSttNd->FindTableNode();
                        const SwTableBox* pBox;
                        if( pTblNd && 0 != ( pBox = pTblNd->GetTable().
                            GetTblBox( pSttNd->GetIndex() ) ) && pBox &&
                            pBox->IsInHeadline( &pTblNd->GetTable() ) )
                            nCond = PARA_IN_TABLEHEAD;
                    }
                    break;
                case SwFlyStartNode:        nCond = PARA_IN_FRAME; break;
                case SwFootnoteStartNode:
                    {
                        nCond = PARA_IN_FOOTENOTE;
                        const SwFtnIdxs& rFtnArr = rNds.GetDoc()->GetFtnIdxs();
                        const SwTxtFtn* pTxtFtn;
                        const SwNode* pSrchNd = pSttNd;

                        for( sal_uInt16 n = 0; n < rFtnArr.size(); ++n )
                            if( 0 != ( pTxtFtn = rFtnArr[ n ])->GetStartNode() &&
                                pSrchNd == &pTxtFtn->GetStartNode()->GetNode() )
                            {
                                if( pTxtFtn->GetFtn().IsEndNote() )
                                    nCond = PARA_IN_ENDNOTE;
                                break;
                            }
                    }
                    break;
                case SwHeaderStartNode:     nCond = PARA_IN_HEADER; break;
                case SwFooterStartNode:     nCond = PARA_IN_FOOTER; break;
                case SwNormalStartNode:     break;
                }
            }

            if( nCond )
            {
                rTmp.SetCondition( (Master_CollConditions)nCond, 0 );
                return true;
            }
            pSttNd = pSttNd->GetIndex()
                        ? pSttNd->StartOfSectionNode()
                        : 0;
        }
    }

    {
        sal_uInt16 nPos;
        const SwOutlineNodes& rOutlNds = rNds.GetOutLineNds();
        if( !rOutlNds.empty() )
        {
            if( !rOutlNds.Seek_Entry( (SwCntntNode*)this, &nPos ) && nPos )
                --nPos;
            if( nPos < rOutlNds.size() &&
                rOutlNds[ nPos ]->GetIndex() < GetIndex() )
            {
                SwTxtNode* pOutlNd = rOutlNds[ nPos ]->GetTxtNode();

                if( pOutlNd->IsOutline())
                {
                    rTmp.SetCondition( PARA_IN_OUTLINE, pOutlNd->GetAttrOutlineLevel() - 1 );
                    return true;
                }
            }
        }
    }

    return false;
}

void SwCntntNode::ChkCondColl()
{
    // Check, just to be sure
    if( RES_CONDTXTFMTCOLL == GetFmtColl()->Which() )
    {
        SwCollCondition aTmp( 0, 0, 0 );
        const SwCollCondition* pCColl;

        bool bDone = false;

        if( IsAnyCondition( aTmp ))
        {
            pCColl = static_cast<SwConditionTxtFmtColl*>(GetFmtColl())
                ->HasCondition( aTmp );

            if (pCColl)
            {
                SetCondFmtColl( pCColl->GetTxtFmtColl() );
                bDone = true;
            }
        }

        if (!bDone)
        {
            if( IsTxtNode() && ((SwTxtNode*)this)->GetNumRule())
            {
                // Is at which Level in a list?
                aTmp.SetCondition( PARA_IN_LIST,
                                ((SwTxtNode*)this)->GetActualListLevel() );
                pCColl = ((SwConditionTxtFmtColl*)GetFmtColl())->
                                HasCondition( aTmp );
            }
            else
                pCColl = 0;

            if( pCColl )
                SetCondFmtColl( pCColl->GetTxtFmtColl() );
            else if( pCondColl )
                SetCondFmtColl( 0 );
        }
    }
}

// #i42921#
short SwCntntNode::GetTextDirection( const SwPosition& rPos,
                                     const Point* pPt ) const
{
    short nRet = -1;

    Point aPt;
    if( pPt )
        aPt = *pPt;

    // #i72024# - No format of the frame, because this can cause recursive layout actions
    SwFrm* pFrm = getLayoutFrm( GetDoc()->GetCurrentLayout(), &aPt, &rPos, false );

    if ( pFrm )
    {
        if ( pFrm->IsVertical() )
        {
            if ( pFrm->IsRightToLeft() )
                nRet = FRMDIR_VERT_TOP_LEFT;
            else
                nRet = FRMDIR_VERT_TOP_RIGHT;
        }
        else
        {
            if ( pFrm->IsRightToLeft() )
                nRet = FRMDIR_HORI_RIGHT_TOP;
            else
                nRet = FRMDIR_HORI_LEFT_TOP;
        }
    }

    return nRet;
}

SwOLENodes* SwCntntNode::CreateOLENodesArray( const SwFmtColl& rColl, bool bOnlyWithInvalidSize )
{
    SwOLENodes *pNodes = 0;
    SwIterator<SwCntntNode,SwFmtColl> aIter( rColl );
    for( SwCntntNode* pNd = aIter.First(); pNd; pNd = aIter.Next() )
    {
        SwOLENode *pONd = pNd->GetOLENode();
        if ( pONd && (!bOnlyWithInvalidSize || pONd->IsOLESizeInvalid()) )
        {
            if ( !pNodes  )
                pNodes = new SwOLENodes;
            pNodes->push_back( pONd );
        }
    }

    return pNodes;
}

//UUUU
drawinglayer::attribute::SdrAllFillAttributesHelperPtr SwCntntNode::getSdrAllFillAttributesHelper() const
{
    return drawinglayer::attribute::SdrAllFillAttributesHelperPtr();
}

/*
 * Document Interface Access
 */
const IDocumentSettingAccess* SwNode::getIDocumentSettingAccess() const { return &GetDoc()->GetDocumentSettingManager(); }
const IDocumentDeviceAccess* SwNode::getIDocumentDeviceAccess() const { return &GetDoc()->getIDocumentDeviceAccess(); }
const IDocumentRedlineAccess* SwNode::getIDocumentRedlineAccess() const { return GetDoc(); }
const IDocumentStylePoolAccess* SwNode::getIDocumentStylePoolAccess() const { return GetDoc(); }
const IDocumentLineNumberAccess* SwNode::getIDocumentLineNumberAccess() const { return GetDoc(); }
const IDocumentDrawModelAccess* SwNode::getIDocumentDrawModelAccess() const { return & GetDoc()->getIDocumentDrawModelAccess(); }
const IDocumentLayoutAccess* SwNode::getIDocumentLayoutAccess() const { return GetDoc(); }
IDocumentLayoutAccess* SwNode::getIDocumentLayoutAccess() { return GetDoc(); }
const IDocumentLinksAdministration* SwNode::getIDocumentLinksAdministration() const { return &GetDoc()->getIDocumentLinksAdministration(); }
IDocumentLinksAdministration* SwNode::getIDocumentLinksAdministration() { return &GetDoc()->getIDocumentLinksAdministration(); }
const IDocumentFieldsAccess* SwNode::getIDocumentFieldsAccess() const { return GetDoc(); }
IDocumentFieldsAccess* SwNode::getIDocumentFieldsAccess() { return GetDoc(); }
IDocumentContentOperations* SwNode::getIDocumentContentOperations() { return &GetDoc()->getIDocumentContentOperations(); }
IDocumentListItems& SwNode::getIDocumentListItems() { return GetDoc()->getIDocumentListItems(); } // #i83479#

const IDocumentMarkAccess* SwNode::getIDocumentMarkAccess() const { return GetDoc()->getIDocumentMarkAccess(); }
IStyleAccess& SwNode::getIDocumentStyleAccess() { return GetDoc()->GetIStyleAccess(); }

bool SwNode::IsInRedlines() const
{
    const SwDoc * pDoc = GetDoc();
    bool bResult = false;

    if (pDoc != NULL)
        bResult = pDoc->IsInRedlines(*this);

    return bResult;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
