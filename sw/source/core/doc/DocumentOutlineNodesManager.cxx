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
#include <DocumentOutlineNodesManager.hxx>
#include <doc.hxx>
#include <ndtxt.hxx>

namespace sw
{

DocumentOutlineNodesManager::DocumentOutlineNodesManager( SwDoc& i_rSwdoc ) : m_rSwdoc( i_rSwdoc )
{
}

sal_Int32 DocumentOutlineNodesManager::getOutlineNodesCount() const
{
    return m_rSwdoc.GetNodes().GetOutLineNds().size();
}

int DocumentOutlineNodesManager::getOutlineLevel( const sal_Int32 nIdx ) const
{
    return m_rSwdoc.GetNodes().GetOutLineNds()[ static_cast<sal_uInt16>(nIdx) ]->
                                GetTxtNode()->GetAttrOutlineLevel()-1;
}

OUString DocumentOutlineNodesManager::getOutlineText( const sal_Int32 nIdx,
                              const bool bWithNumber,
                              const bool bWithSpacesForLevel,
                              const bool bWithFtn ) const
{
    return m_rSwdoc.GetNodes().GetOutLineNds()[ static_cast<sal_uInt16>(nIdx) ]->
                GetTxtNode()->GetExpandTxt( 0, -1, bWithNumber,
                                            bWithNumber, bWithSpacesForLevel, bWithFtn );
}

SwTxtNode* DocumentOutlineNodesManager::getOutlineNode( const sal_Int32 nIdx ) const
{
    return m_rSwdoc.GetNodes().GetOutLineNds()[ static_cast<sal_uInt16>(nIdx) ]->GetTxtNode();
}

void DocumentOutlineNodesManager::getOutlineNodes( IDocumentOutlineNodes::tSortedOutlineNodeList& orOutlineNodeList ) const
{
    orOutlineNodeList.clear();
    orOutlineNodeList.reserve( getOutlineNodesCount() );

    const sal_uInt16 nOutlCount( static_cast<sal_uInt16>(getOutlineNodesCount()) );
    for ( sal_uInt16 i = 0; i < nOutlCount; ++i )
    {
        orOutlineNodeList.push_back(
            m_rSwdoc.GetNodes().GetOutLineNds()[i]->GetTxtNode() );
    }
}

DocumentOutlineNodesManager::~DocumentOutlineNodesManager()
{
}


}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
