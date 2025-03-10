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

#ifndef INCLUDED_DBACCESS_SOURCE_UI_DLG_CONNECTIONPAGE_HXX
#define INCLUDED_DBACCESS_SOURCE_UI_DLG_CONNECTIONPAGE_HXX

#include "ConnectionHelper.hxx"
#include "adminpages.hxx"
#include <ucbhelper/content.hxx>
#include "curledit.hxx"

namespace dbaui
{

    // OConnectionTabPage

    /** implements the connection page of the data source properties dialog.
    */
    class OConnectionTabPage : public OConnectionHelper
    {
    protected:
        // user authentification
        FixedText*          m_pFL2;
        FixedText*          m_pUserNameLabel;
        Edit*               m_pUserName;
        CheckBox*           m_pPasswordRequired;

        // jdbc driver
        FixedText*          m_pFL3;
        FixedText*          m_pJavaDriverLabel;
        Edit*               m_pJavaDriver;
        PushButton*         m_pTestJavaDriver;

        // connection test
        PushButton*         m_pTestConnection;

        // called when the test connection button was clicked
        DECL_LINK(OnTestJavaClickHdl,PushButton*);
        DECL_LINK(OnEditModified,Edit*);

    public:
        static  SfxTabPage* Create( Window* pParent, const SfxItemSet* _rAttrSet );
        virtual bool        FillItemSet (SfxItemSet* _rCoreAttrs) SAL_OVERRIDE;

        virtual void        implInitControls(const SfxItemSet& _rSet, bool _bSaveValue) SAL_OVERRIDE;

        inline void enableConnectionURL() { m_pConnectionURL->SetReadOnly(false); }
        inline void disableConnectionURL() { m_pConnectionURL->SetReadOnly(); }

        /** changes the connection URL.
            <p>The new URL must be of the type which is currently selected, only the parts which do not
            affect the type may be changed (compared to the previous URL).</p>
        */
    private:
        OConnectionTabPage(Window* pParent, const SfxItemSet& _rCoreAttrs);
            // nControlFlags is a combination of the CBTP_xxx-constants
        virtual ~OConnectionTabPage();

    private:
        /** enables the test connection button, if allowed
        */
        virtual bool checkTestConnection() SAL_OVERRIDE;
    };
}   // namespace dbaui

#endif // INCLUDED_DBACCESS_SOURCE_UI_DLG_CONNECTIONPAGE_HXX

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
