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
#ifndef INCLUDED_SW_SOURCE_UIBASE_INC_SWLBOX_HXX
#define INCLUDED_SW_SOURCE_UIBASE_INC_SWLBOX_HXX

#include <vcl/lstbox.hxx>
#include <vcl/combobox.hxx>
#include "swdllapi.h"
#include <boost/ptr_container/ptr_vector.hpp>

class SwBoxEntry;
class Window;

typedef boost::ptr_vector<SwBoxEntry> SwEntryLst;

class SW_DLLPUBLIC SwBoxEntry
{
    friend class SwComboBox;

    bool    bModified : 1;
    bool    bNew : 1;

    OUString    aName;
    sal_Int32   nId;

public:
    SwBoxEntry(const OUString& aName, sal_Int32 nId=0);
    SwBoxEntry(const SwBoxEntry& rOrg);
    SwBoxEntry();

    const OUString& GetName() const { return aName;}
};

// for combo boxes
class SW_DLLPUBLIC SwComboBox : public ComboBox
{
    SwEntryLst              aEntryLst;
    SwEntryLst              aDelEntryLst;
    SwBoxEntry              aDefault;

    SAL_DLLPRIVATE void InitComboBox();
    SAL_DLLPRIVATE void InsertSorted(SwBoxEntry* pEntry);
    SAL_DLLPRIVATE void Init();

public:

    SwComboBox(Window* pParent, WinBits nStyle);
    virtual ~SwComboBox();

    void                    InsertSwEntry(const SwBoxEntry&);
    virtual sal_Int32       InsertEntry(const OUString& rStr, sal_Int32 = 0) SAL_OVERRIDE;

    virtual void            RemoveEntryAt(sal_Int32 nPos) SAL_OVERRIDE;

    sal_Int32               GetSwEntryPos(const SwBoxEntry& rEntry) const;
    const SwBoxEntry&       GetSwEntry(sal_Int32) const;

    sal_Int32               GetRemovedCount() const;
    const SwBoxEntry&       GetRemovedEntry(sal_Int32 nPos) const;
};

#endif // INCLUDED_SW_SOURCE_UIBASE_INC_SWLBOX_HXX

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
