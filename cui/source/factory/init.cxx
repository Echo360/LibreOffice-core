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

#include <vcl/msgbox.hxx>
#include "cuicharmap.hxx"
#include <boost/scoped_ptr.hpp>

// hook to call special character dialog for edits
// caution: needs C-Linkage since dynamically loaded via symbol name
extern "C"
{
SAL_DLLPUBLIC_EXPORT bool GetSpecialCharsForEdit(Window* i_pParent, const Font& i_rFont, OUString& o_rResult)
{
    bool bRet = false;
    boost::scoped_ptr<SvxCharacterMap> aDlg(new SvxCharacterMap( i_pParent ));
    aDlg->DisableFontSelection();
    aDlg->SetCharFont(i_rFont);
    if ( aDlg->Execute() == RET_OK )
    {
        o_rResult = aDlg->GetCharacters();
        bRet = true;
    }
    return bRet;
}
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
