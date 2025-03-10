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

#ifndef INCLUDED_SD_SOURCE_UI_INC_FUOUTL_HXX
#define INCLUDED_SD_SOURCE_UI_INC_FUOUTL_HXX

#include "fupoor.hxx"

class SdDrawDocument;
class SfxRequest;

namespace sd {

class OutlineView;
class OutlineViewShell;
class View;
class ViewShell;
class Window;

/**
 * Base class of functions of outline mode
 */
class FuOutline
    : public FuPoor
{
public:
    TYPEINFO_OVERRIDE();

    virtual bool Command(const CommandEvent& rCEvt) SAL_OVERRIDE;

protected:
    FuOutline (
        ViewShell* pViewShell,
        ::sd::Window* pWindow,
        ::sd::View* pView,
        SdDrawDocument* pDoc,
        SfxRequest& rReq);

    OutlineViewShell* pOutlineViewShell;
    OutlineView* pOutlineView;
};

} // end of namespace sd

#endif

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
