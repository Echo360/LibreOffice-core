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

#ifndef INCLUDED_SD_SOURCE_UI_INC_INSPAGOB_HXX
#define INCLUDED_SD_SOURCE_UI_INC_INSPAGOB_HXX

#include <vcl/button.hxx>
#include <vcl/dialog.hxx>
#include "sdtreelb.hxx"

class SdDrawDocument;

class SdInsertPagesObjsDlg : public ModalDialog
{
private:
    SdPageObjsTLB*          m_pLbTree;
    CheckBox*               m_pCbxLink;
    CheckBox*               m_pCbxMasters;

    SfxMedium*              pMedium;
    const SdDrawDocument*   mpDoc;
    const OUString&         rName;

    void                    Reset();
    DECL_LINK( SelectObjectHdl, void * );

public:
                SdInsertPagesObjsDlg( Window* pParent,
                                const SdDrawDocument* pDoc,
                                SfxMedium* pSfxMedium,
                                const OUString& rFileName );
                virtual ~SdInsertPagesObjsDlg();

    /** returns the list
          nType == 0 -> pages
          nType == 1 -> objects    */

    std::vector<OUString> GetList ( const sal_uInt16 nType ) ;

    bool        IsLink();
    bool        IsRemoveUnnessesaryMasterPages() const;
};

#endif // INCLUDED_SD_SOURCE_UI_INC_INSPAGOB_HXX

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
