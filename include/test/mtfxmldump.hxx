/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef INCLUDED_TEST_MTFXMLDUMP_HXX
#define INCLUDED_TEST_MTFXMLDUMP_HXX

#include <sal/config.h>
#include <test/testdllapi.hxx>

#include <libxml/tree.h>
#include <vcl/gdimtf.hxx>
#include <vector>

class OOO_DLLPUBLIC_TEST MetafileXmlDump
{
    std::vector<bool> maFilter;

public:
    MetafileXmlDump();
    virtual ~MetafileXmlDump();

    void filterActionType(const sal_uInt16 nActionType, bool bShouldFilter);
    void filterAllActionTypes();

    /** The actual result that will be used for testing.

        This function normally uses a SvMemoryStream for its operation; but
        can use a physical file when a filename is specified in
        pTempStreamName - this is useful when creating the test, to dump the
        file for examination.
    */
    xmlDocPtr dumpAndParse(GDIMetaFile& rMetaFile, const OUString& rTempStreamName = OUString());
};

#endif

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
